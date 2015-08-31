#ifndef TS_VECTOR_H
#define TS_VECTOR_H

#include <pthread.h>

#include <vector>
#include <string>

using std::vector;
using std::string;

/* A thread-safe vector */
template <typename T>
class ts_vector
{
public:
	ts_vector()
	{
		if (pthread_mutex_init(&lock, NULL)) {
			throw -1;
		}
	}

	void push_back(T& item)
	{
		pthread_mutex_lock(&lock);
		v.push_back(item);
		pthread_mutex_unlock(&lock);
	}

	bool contains(T& item)
	{
		pthread_mutex_lock(&lock);
		bool result = find(begin(v), end(v), item) != end(v);
		pthread_mutex_unlock(&lock);
		return result;
	}

	int remove(T& item)
	{
		int result = 0;

		pthread_mutex_lock(&lock);
		auto iter = find(begin(v), end(v), item);
		if (iter == end(v))
			result = -1;
		else
			v.erase(iter);
		pthread_mutex_unlock(&lock);
		return result;
	}

private:
	vector<T>	v;
	pthread_mutex_t	lock;
};

#endif
