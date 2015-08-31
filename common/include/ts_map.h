#ifndef TS_MAP_H
#define TS_MAP_H

#include <pthread.h>

#include <map>

using std::map;

/* A thread-safe map */
template <typename K, typename T>
class ts_map
{
public:
	ts_map()
	{
		if (pthread_mutex_init(&lock, NULL)) {
			throw -1;
		}
	}

	void add(K& key, T& item)
	{
		pthread_mutex_lock(&lock);
		m[key] = item;
		pthread_mutex_unlock(&lock);
	}

	bool contains(K& key)
	{
		pthread_mutex_lock(&lock);
		bool result = m.find(key) != end(m);
		pthread_mutex_unlock(&lock);
		return result;
	}

	int remove(K& key)
	{
		int result = 0;

		pthread_mutex_lock(&lock);
		auto iter = m.find(key);
		if (iter == end(m))
			result = -1;
		else
			m.erase(iter);
		pthread_mutex_unlock(&lock);
		return result;
	}

	T get_item(K& key)
	{
		T	item;
		pthread_mutex_lock(&lock);
		auto iter = m.find(key);
		item = iter->second;
		pthread_mutex_unlock(&lock);
		return item;
	}

private:
	map<K,T>	m;
	pthread_mutex_t	lock;
};

#endif

