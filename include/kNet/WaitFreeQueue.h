/* Copyright 2010 Jukka Jyl�nki

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */
#pragma once

/** @file WaitFreeQueue.h
	@brief The WaitFreeQueue<T> template class. */

#include "Alignment.h"

namespace kNet
{

/// A wait-free queue for communication unidirectionally between two threads.
/** This data structure is useful for simple and efficient lockless data/message passing between two-threads.
	It is implemented using a lockless circular ring buffer, and has the following properties:
	 - At most one thread can act as a publisher/producer/writer and call Insert() to add new items to the queue.
	 - At most one thread can consume/read the items from the queue by calling Front() and PopFront().
	 - Does not use locks or spin-waits, and is hence wait-free.
	 - Does not perform any memory allocation after initialization.
	 - Only POD types are supported. If you need non-POD objects, store pointers to objects instead.
	 - The queue has a fixed upper size that must be a power-of-2 and must be speficied in the constructor. */
template<typename T>
class WaitFreeQueue
{
public:
	/// @param maxElements A power-of-2 number, > 2,  that specifies the size of the ring buffer to construct. The number of elements the queue can store is maxElements-1.
	explicit WaitFreeQueue(size_t maxElements)
	:head(0), tail(0)
	{
		assert(IS_POW2(maxElements)); // The caller really needs to round to correct pow2,
		assert(maxElements > 2);
		maxElements = (size_t)RoundUpToNextPow2((u32)maxElements); // but avoid any silliness in release anyways.

		data = new T[maxElements];
		maxElementsMask = maxElements - 1;
	}

	~WaitFreeQueue()
	{
		delete[] data;
	}

	/// Returns the total capacity of the queue, i.e. the total maximum number of items that can be contained in it.
	/// Thread-safe.
	int Capacity() const { return maxElementsMask; }

	/// Returns the number of items that can still be inserted in the queue, i.e. the total space left.
	/// Thread-safe.
	int CapacityLeft() const { return Capacity() - Size(); }

	/// Inserts the new value into the queue. Can be called only by a single producer thread.
	bool Insert(const T &value)
	{
		// Inserts are made to the 'tail' of the queue, incrementing the tail index.
		unsigned long tail_ = tail;
		unsigned long nextTail = (tail_ + 1) & maxElementsMask;
		if (nextTail == head)
			return false;
		data[tail_] = value;
		tail = nextTail;

		return true;
	}

	/// Inserts the new value into the queue.
	/// \note This function is not thread-safe. Do not call this if you cannot guarantee that the other
	///       thread will not be accessing the queue at the same time.
	void InsertWithResize(const T &value)
	{
		bool success = Insert(value);
		if (!success)
		{
			DoubleCapacity();
			success = Insert(value);
		}
		assert(success);
	}

	/// Re-allocates the queue to the new maximum size. All old elements are copied over.
	/// \note This function is not thread-safe. Do not call this if you cannot guarantee that the other
	///       thread will not be accessing the queue at the same time.
	void Resize(size_t newSize)
	{
		assert(IS_POW2(newSize)); // The caller really needs to round to correct pow2,
		newSize = (size_t)RoundUpToNextPow2((u32)newSize); // but avoid any silliness in release anyways.

		T *newData = new T[newSize];
		unsigned long newTail = 0;
		for(unsigned long i = 0; i < Size(); ++i)
			newData[newTail++] = *ItemAt(i);
		delete[] data;
		data = newData;
		head = 0;
		tail = newTail;
		maxElementsMask = newSize - 1;
	}

	/// Resizes this queue to hold twice the amount of maximum elements.
	/// \note This function is not thread-safe. Do not call this if you cannot guarantee that the other
	///       thread will not be accessing the queue at the same time.
	void DoubleCapacity() { Resize(2*(maxElementsMask+1)); }

	/// Returns a pointer to the first item in the queue. Can be called only from a single consumer thread.
	T *Front()
	{
		if (head == tail)
			return 0;
		return &data[head];
	}

	/// Returns a pointer to the item at the given index. Can be called only from a single consumer thread.
	T *ItemAt(int index)
	{
		assert(index < (int)Size());
		return &data[(head + index) & maxElementsMask];
	}

	/// Returns a pointer to the item at the given index. Can be called only from a single consumer thread.
	const T *ItemAt(int index) const
	{
		assert(index < (int)Size());
		return &data[(head + index) & maxElementsMask];
	}

	/// Returns true if the given element exists in the queue. Can be called only from a single consumer thread.
	bool Contains(const T &item) const
	{
		for(int i = 0; i < (int)Size(); ++i)
			if (*ItemAt(i) == item)
				return true;
		return false;
	}

	/// Removes the element at the given index.
	///\note Not thread-safe.
	void EraseItemAt(int index)
	{
		if (index <= Size()>>1)
			EraseItemAtMoveFront(index);
		else
			EraseItemAtMoveBack(index);
	}

	/// Returns a pointer to the first item in the queue. Can be called only from a single consumer thread.
	const T *Front() const
	{
		if (head == tail)
			return 0;
		return &data[head];
	}

	/// Returns a copy of the first item in the queue and pops that item off the queue. Can be called only from a single consumer thread.
	T TakeFront()
	{
		T frontVal = *Front();
		PopFront();
		return frontVal;
	}

	/// Removes all elements in the queue. Does not call dtors for removed objects, as this queue is only for POD types.
	/// Can be called only from a single consumer thread.
	void Clear()
	{
		tail = head;
	}

	/// Returns the number of elements currently filled in the queue. Can be called from either the consumer or producer thread.
	unsigned long Size() const
	{
		unsigned long head_ = head;
		unsigned long tail_ = tail;
		if (tail_ >= head_)
			return tail_ - head_;
		else
			return maxElementsMask + 1 - (head_ - tail_);
	}

	/// Removes the first item in the queue. Can be called only from a single consumer thread.
	void PopFront()
	{
		assert(head != tail);
		if (head == tail)
			return;
		size_t head_ = (head + 1) & maxElementsMask;
		head = head_;
	}

private:
	T *data;
	/// Stores the AND mask (2^Size-1) used to perform the modulo check.
	unsigned long maxElementsMask;
	/// Stores the index of the first element in the queue. The next item to come off the queue is at this position,
	/// unless head==tail, and the queue is empty. \todo Convert to C++0x atomic<unsigned long> head;
	volatile unsigned long head;
	/// Stores the index of one past the last element in the queue. \todo Convert to C++0x atomic<unsigned long> head;
	volatile unsigned long tail; 

	WaitFreeQueue(const WaitFreeQueue<T> &rhs);
	void operator=(const WaitFreeQueue<T> &rhs);

	/// Removes the element at the given index, but instead of filling the contiguous gap that forms by moving elements to the
	/// right, this function will instead move items at the front of the queue.
	///\note Not thread-safe.
	void EraseItemAtMoveFront(int index)
	{
		assert(Size() > 0);
		int numItemsToMove = index;
		for(int i = 0; i < numItemsToMove; ++i)
			data[(head+index + maxElementsMask+1 -i)&maxElementsMask] = data[(head+index + maxElementsMask+1 -i-1) &maxElementsMask];
		head = (head+1) & maxElementsMask;
	}

	/// Removes the element at the given index, and fills the contiguous gap that forms by shuffling each item after index one space down.
	///\note Not thread-safe.
	void EraseItemAtMoveBack(int index)
	{
		assert(Size() > 0);
		int numItemsToMove = Size()-1-index;
		for(int i = 0; i < numItemsToMove; ++i)
			data[(head+index+i)&maxElementsMask] = data[(head+index+i+1)&maxElementsMask];
		tail = (tail + maxElementsMask+1 - 1) & maxElementsMask;
	}
};

/// Checks that the specified conditions for the container apply.
/// Warning: This is a non-threadsafe check for the container, only to be used for debugging.
/// Warning #2: This function is very slow, as it performs a N^2 search through the container.
template<typename T>
bool ContainerUniqueAndNoNullElements(const WaitFreeQueue<T> &cont)
{
	for(size_t i = 0; i < cont.Size(); ++i)
		for(size_t j = i+1; j < cont.Size(); ++j)
			if (*cont.ItemAt(i) == *cont.ItemAt(j) || *cont.ItemAt(i) == 0)
				return false;
	return true;
}

} // ~kNet
