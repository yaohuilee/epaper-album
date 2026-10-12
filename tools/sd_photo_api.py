"""Network SD photo client; standard library only, works on native Windows Python."""
import argparse
from http.client import IncompleteRead
import json
import os
from pathlib import Path
import sys
import time
from urllib.error import HTTPError
from urllib.parse import urlencode, urlsplit
from urllib.request import Request, build_opener, ProxyHandler


class Api:
    def __init__(self, url):
        parsed = urlsplit(url)
        if parsed.scheme != "http" or not parsed.hostname or parsed.username or parsed.path not in ("", "/"):
            raise ValueError("Use a device base URL such as http://192.168.1.100")
        self.base = url.rstrip("/")
        self.opener = build_opener(ProxyHandler({}))
        self.status = self.read_status()
        self.token = self.status["control"]["token"]

    def open(self, path, method="GET", data=None, content_type=None):
        headers = {"X-Album-Token": self.token}
        if content_type:
            headers["Content-Type"] = content_type
        deadline = time.monotonic() + 120
        # Upload completion includes SD flush and full six-color validation.
        # A response can take longer than a small status request.
        timeout = 120 if method == 'PUT' and path.startswith('/api/photo?') else 15
        discarded_retries = 0
        while True:
            try:
                return self.opener.open(Request(self.base + path, data=data, headers=headers, method=method), timeout=timeout)
            except HTTPError as exc:
                detail = exc.read(4096).decode("utf-8", "replace")
                # A busy rejection happens before changing the SD. Never retry
                # an ambiguous timeout or a conflicting existing filename.
                try:
                    error = json.loads(detail).get('error')
                    busy = exc.code == 409 and error == 'busy'
                    discarded = (exc.code == 408 and error == 'upload_incomplete' and
                                 method == 'PUT' and path.startswith('/api/photo?'))
                except (ValueError, AttributeError):
                    busy = discarded = False
                if busy and time.monotonic() < deadline:
                    time.sleep(0.5)
                    continue
                # The server explicitly confirms removal of partial bytes.
                # This does not include ambiguous socket/client timeouts.
                if discarded and discarded_retries < 3:
                    discarded_retries += 1
                    time.sleep(discarded_retries)
                    continue
                raise RuntimeError(f"HTTP {exc.code}: {detail}") from exc

    def read_status(self):
        with self.opener.open(self.base + "/api/status", timeout=15) as response:
            return json.load(response)

    def json(self, path, method="GET", data=None, content_type=None):
        with self.open(path, method, data, content_type) as response:
            return json.load(response)

    def download_bytes(self, path, expected_bytes=None):
        """Retry interrupted read-only transfers; never repeat a write here."""
        for attempt in range(4):
            try:
                self.wait_ready()
                with self.open(path) as response:
                    content = response.read()
                if expected_bytes is not None and len(content) != expected_bytes:
                    raise IncompleteRead(content, expected_bytes-len(content))
                return content
            except (IncompleteRead, OSError):
                if attempt == 3:
                    raise
                time.sleep(2 ** attempt)

    def wait_index(self, previous_revision):
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            state = self.read_status()["control"]
            if state["files_revision"] > previous_revision and not state["index_pending"]:
                if state["index_error"]:
                    raise RuntimeError("SD file changed, but index update failed: " + state["index_error"])
                return
            time.sleep(0.5)
        raise RuntimeError("SD file changed; index completion was not confirmed. Check /api/status before retrying.")

    def wait_ready(self):
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            status = self.read_status()
            control = status['control']
            if not control['busy'] and not control['index_pending']:
                if control['index_error']:
                    raise RuntimeError('Index error: ' + control['index_error'])
                return status
            time.sleep(0.5)
        raise RuntimeError('Device remains busy; no new mutation was sent')

    def command(self, path, payload):
        self.wait_ready()
        result = self.json(path, 'POST', json.dumps(payload).encode(), 'application/json')
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            status = self.read_status()
            control = status['control']
            if control['job_id'] == result['job_id']:
                if control['state'] == 'succeeded':
                    return status
                if control['state'] == 'failed':
                    raise RuntimeError(control['error'])
            elif control['job_id'] > result['job_id']:
                raise RuntimeError('Another client submitted a command; verify outcome before retrying')
            time.sleep(0.5)
        raise RuntimeError('Command completion was not confirmed; verify outcome before retrying')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True, help="Device HTTP base URL")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("albums", help="List configured album directories")
    create = commands.add_parser('create-album', help='Create a new SD directory and manifest entry')
    create.add_argument('album')
    create.add_argument('--enabled', action='store_true')
    create.add_argument('--interval', type=int, default=900)
    remove_album = commands.add_parser('delete-album', help='Remove an empty SD album directory and manifest entry')
    remove_album.add_argument('album')
    listing = commands.add_parser("list", help="List one album's BMP files")
    listing.add_argument("album")
    listing.add_argument("--offset", type=int, default=0)
    listing.add_argument("--limit", type=int, default=50)
    upload = commands.add_parser("upload", help="Upload a preprocessed BMP; existing files are not overwritten")
    upload.add_argument("album")
    upload.add_argument("file", type=Path)
    upload.add_argument("--name", help="SD filename; defaults to local filename")
    download = commands.add_parser("download", help="Download exact original bytes to a new local file")
    download.add_argument("album")
    download.add_argument("name")
    download.add_argument("destination", type=Path)
    delete = commands.add_parser("delete", help="Permanently remove this named SD photo")
    delete.add_argument("album")
    delete.add_argument("name")
    args = parser.parse_args(argv)
    api = Api(args.url)
    if args.command == "albums":
        result = api.json("/api/albums")
    elif args.command == 'create-album':
        api.command('/api/album-create', {'schema_version': 1, 'id': args.album, 'enabled': args.enabled, 'interval_seconds': args.interval})
        result = {'created': args.album}
    elif args.command == 'delete-album':
        api.command('/api/album-delete', {'schema_version': 1, 'id': args.album})
        result = {'deleted': args.album}
    elif args.command == "list":
        result = api.json("/api/files?" + urlencode({"album": args.album, "offset": args.offset, "limit": args.limit}))
    else:
        name = (args.name or args.file.name) if args.command == "upload" else args.name
        path = "/api/photo?" + urlencode({"album": args.album, "name": name})
        if args.command == "download":
            destination = args.destination
            if destination.exists():
                raise FileExistsError("Destination already exists; choose a new path")
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_name(destination.name + ".part")
            created = False
            try:
                with temporary.open("xb") as output:
                    created = True
                    with api.open(path) as response:
                        while chunk := response.read(65536):
                            output.write(chunk)
                    output.flush()
                    os.fsync(output.fileno())
                # Windows rename also refuses a destination created during the transfer.
                if destination.exists():
                    raise FileExistsError("Destination was created during download")
                temporary.rename(destination)
            finally:
                if created and temporary.exists():
                    temporary.unlink()
            result = {"downloaded": str(destination), "bytes": destination.stat().st_size}
        else:
            before = api.read_status()["control"]["files_revision"]
            if args.command == "upload":
                size = args.file.stat().st_size
                if not 54 <= size <= 2 * 1024 * 1024:
                    raise ValueError("BMP size must be 54 bytes..2 MiB")
                result = api.json(path, "PUT", args.file.read_bytes(), "image/bmp")
            else:
                result = api.json(path, "DELETE")
            api.wait_index(before)
            result["index_pending"] = False
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
