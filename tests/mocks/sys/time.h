#pragma once
struct timeval {long long tv_sec;long tv_usec;};
int gettimeofday(struct timeval *t,void *unused);
