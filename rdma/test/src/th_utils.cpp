#include <vector>
#include <iterator>
#include <iostream>
#include <random>
#include <algorithm>

#include <time.h>

using std::cout;
using std::endl;
using std::vector;

template <typename T>
class test_items 
{
public:
	void shuffle()
	{
		std::srand(time(0));
		std::random_shuffle(begin(the_list), end(the_list));
	}

	size_t	size() const { return the_list.size(); }	
	
	T operator [](size_t i) { return the_list[i]; }


protected:
	vector<T>	the_list;
};

template <typename T>
class blocks : public test_items<T>
{
public:
	/**
	 * Generate block sizes starting from total_size /2
	 * and continuing with halfing, until the min_block_size
	 * is reached.
	 */
	void generate(T total_size, T min_block_size)
	{
		auto size = total_size / 2;

		while (size >= min_block_size) {
			test_items<T>::the_list.push_back(size);
			size /= 2;
		}
		test_items<T>::the_list.push_back(size * 2);
	}
};

template <typename T>
class offsets : public test_items<T>
{
public:
	void generate(T total_size, T step_size, T data_size)
	{
		T	offset = 0;	/* Start at offset 0 */

		while ((offset + data_size) <= total_size) {
			test_items<T>::the_list.push_back(offset);
			offset += step_size;
		}
	}
	vector<T>	offset_list;
};

#define SYNC 1
#define ASYNC 2
#define FAF 4 
/**
 * This class is used for discrete values such as DMA write modes
 * which can be duplicated to a certain length, and shuffled.
 */
template <typename T>
class discretes : public test_items<T>
{
public:
	discretes()
	{
		std::srand(time(NULL));
	}

	T get_random()
	{
		size_t ridx = std::rand()%(test_items<T>::the_list.size());
		return test_items<T>::the_list[ridx];
	}
	void push_back(T item){ test_items<T>::the_list.push_back(item); }
};

blocks<uint32_t>	ms_blocks;

offsets<uint32_t>	msub_offsets;

discretes<uint32_t>  dma_modes;

int main()
{
	ms_blocks.generate(1024, 4);

	for (unsigned i = 0; i < ms_blocks.size(); i++) {
		cout << "ms_size[" << i << "] = " << ms_blocks[i] << endl;
	}

	cout << "Now shuffle the list" << endl;
	ms_blocks.shuffle();
	for (unsigned i = 0; i < ms_blocks.size(); i++) {
		cout << "ms_size[" << i << "] = " << ms_blocks[i] << endl;
	}

	msub_offsets.generate(4096, 256, 1024);
	for (unsigned i = 0; i < msub_offsets.size(); i++) {
		cout << "msub_offset[" << i << "] = " << msub_offsets[i] << endl;
	}

	cout << "Now shuffle the list" << endl;
	msub_offsets.shuffle();
	for (unsigned i = 0; i < msub_offsets.size(); i++) {
		cout << "msub_offset[" << i << "] = " << msub_offsets[i] << endl;
	}

	dma_modes.push_back(SYNC);
	dma_modes.push_back(ASYNC);
	dma_modes.push_back(FAF);
	for (unsigned i = 0; i < 20; i++) {
		cout << "Random DMA mode = " << dma_modes.get_random() << endl;
	}
	return 0;
}
