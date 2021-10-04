#include <iostream>
#include "processor.hpp"

EventQueue event_queue;

[[ noreturn ]]
void RunProcessor(void *arg) {
    std::cout << "started event processor" << std::endl;
    while(1) {
        event_queue.WaitForReadEvent();
        while (!event_queue.IsQueueEmpty()) {
            auto event_base = event_queue.ReadElementFromQueue();
            std::cout << "Event read" << std::endl;
            if (event_base == nullptr) {
                std::cout << "Event read as empty" << std::endl;
            }
            switch(event_base->event_type) {
                case E_RPC1:
                    {
                        // Handle the event
                        RPC1Event *rpc1_event = dynamic_cast<RPC1Event*>(event_base);
                        if (rpc1_event) {
                            std::cout << "Handling rpc1 event" << std::endl;
                            auto &value = rpc1_event->request->value;
                            std::cout << "Found value " <<  value << std::endl;
                            auto rsp = std::make_shared<RPC1AppMsgResponse>();
                            rsp->value = std::string(value);
                            std::cout << "About to call reponse_fn" << std::endl;
                            rpc1_event->response_fn(rsp, true);
                            std::cout << "called reponse_fn" << std::endl;
                        }
                    }
                    break;
                default:
                    std::cout << "Unknown event. Dropping event" << std::endl;
            }
            delete event_base;
        }
    }
    std::cout << "Ended started event processor" << std::endl;
}
