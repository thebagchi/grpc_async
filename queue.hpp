#ifndef QUEUE_H
#define QUEUE_H

#include <memory>
#include <mutex>
#include <condition_variable>

#define SIZE 1024

template<typename T>
struct Queue {
    uint32_t cons_idx = 0;
    uint32_t prod_idx = 0;
    std::mutex _lck;
    std::condition_variable cv;
    std::array<T*, SIZE> elems{};

private:
    uint32_t _SlotsForRead() {
        uint32_t slots = 0;
        if (prod_idx > cons_idx) {
            slots = prod_idx - cons_idx;
        } else if (prod_idx == cons_idx) {
            slots = 0;
        } else {
            slots = (SIZE - cons_idx) + prod_idx;
        }
        return slots;
    }

    uint32_t _SlotsForWrite() {
        uint32_t slots = 0;
        if (prod_idx > cons_idx) {
            slots = (SIZE - prod_idx - 1) + cons_idx;
        } else if (prod_idx == cons_idx) {
            return SIZE - 1;
        } else {
            slots = cons_idx - prod_idx - 1;
        }
        return slots;
    }

public:
    bool IsQueueFull() {
        std::lock_guard<std::mutex> guard(_lck);
        if ((prod_idx + 1) % SIZE == cons_idx) {
            return true;
        } else {
            return false;
        }
    }
    
    bool IsQueueEmpty() {
        std::lock_guard<std::mutex> guard(_lck);
        if (prod_idx == cons_idx) {
            return true;
        } else {
            return false;
        }
    }
    
    uint32_t SlotsAvailableForWrite() {
        std::lock_guard<std::mutex> guard(_lck);
        return _SlotsForWrite();
    }
    
    uint32_t SlotsAvailableForRead() {
        std::lock_guard<std::mutex> guard(_lck);
        return _SlotsForRead();
    }
    
    int PutElementInQueue(T *data) {
        std::cout << "putting element in queue " << std::endl;
        std::lock_guard<std::mutex> guard(_lck);
        if (_SlotsForWrite()) {
            prod_idx = (prod_idx + 1) % SIZE;
            elems[prod_idx] = data;
            cv.notify_one();
            return 0;
        }
        return -1;
    }

    T* ReadElementFromQueue() {
        std::cout << "Read element from queue" << std::endl;
        std::lock_guard<std::mutex> guard(_lck);
        if (_SlotsForRead()) {
            cons_idx = (cons_idx + 1) % SIZE;
            std::cout << "Getting element " << std::endl;
            return elems[cons_idx];
        }
        return nullptr;
    }
    void WaitForReadEvent() {
        std::unique_lock<std::mutex> unq(_lck);
        cv.wait(unq, [this]{
                    return (_SlotsForRead() > 0);
                });
    }
};
#endif
