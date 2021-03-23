//
// Created by Igor Frank on 23.03.21.
//

#ifndef VULKANAPP_EXCEPTIONS_H
#define VULKANAPP_EXCEPTIONS_H

class NotImplementedException : public std::logic_error
{
 public:
  NotImplementedException() : std::logic_error("Function not yet implemented") { };
};
#endif//VULKANAPP_EXCEPTIONS_H
