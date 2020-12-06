//
// Created by Igor Frank on 30.10.20.
//

#ifndef VULKANAPP_BUFFER_H
#define VULKANAPP_BUFFER_H


#include "Device.h"
#include "Types.h"
#include "builders/BufferBuilder.h"
#include <span>

template<typename T>
concept Pointer = std::is_pointer_v<T>;

template<typename T>
concept RawDataProvider = requires(T t) {
    { t.data() }
    ->Pointer;
    { t.size() }
    ->std::convertible_to<std::size_t>;
};

template<Pointer T>
using ptr_val = decltype(*std::declval<T>());

class Buffer {
public:
    Buffer(BufferBuilder builder, std::shared_ptr<Device> device, const vk::UniqueCommandPool &commandPool, const vk::Queue &queue);


    void fill(const RawDataProvider auto &input, bool useStaging = true) {
        assert(size >= input.size());
        using ValueType = ptr_val<decltype(input.data())>;
        auto fillSize = sizeof(ValueType) * input.size();

        if(useStaging) {
            auto stagingBuffer = Buffer(BufferBuilder()
                                                .setSize(fillSize)
                                                .setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
                                                .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible),
                                        device, commandPool, queue);

            auto data = device->getDevice()->mapMemory(stagingBuffer.getDeviceMemory().get(), 0, fillSize);
            memcpy(data, input.data(), fillSize);
            device->getDevice()->unmapMemory(stagingBuffer.getDeviceMemory().get());

            copy(fillSize, stagingBuffer);
        } else {
            auto data = device->getDevice()->mapMemory(deviceMemory.get(), 0, fillSize);
            memcpy(data, input.data(), fillSize);
            device->getDevice()->unmapMemory(deviceMemory.get());
        }
    };

    void copy(const vk::DeviceSize &copySize, const Buffer &srcBuffer, const std::vector<vk::Semaphore> &semaphores = {});
    void copy(const vk::DeviceSize &copySize, const Buffer &srcBuffer, const Buffer &dstBuffer, const std::vector<vk::Semaphore> &semaphores = {});
    template<typename T>
    [[nodiscard]] std::vector<T> read() {
        auto stagingBuffer = Buffer(BufferBuilder()
                                            .setSize(size)
                                            .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst)
                                            .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible),
                                    device, commandPool, queue);

        stagingBuffer.copy(size, *this);

        std::vector<T> data{};
        data.resize(size / sizeof(T));
        auto bufferData = device->getDevice()->mapMemory(stagingBuffer.deviceMemory.get(), 0, size);
        memcpy(data.data(), bufferData, size);
        device->getDevice()->unmapMemory(stagingBuffer.deviceMemory.get());
        return data;
    }
    [[nodiscard]] const vk::UniqueBuffer &getBuffer() const;
    [[nodiscard]] const vk::UniqueDeviceMemory &getDeviceMemory() const;

private:
    void copyBuffer(const vk::DeviceSize &copySize, const vk::UniqueBuffer &srcBuffer, const vk::UniqueBuffer &dstBuffer, const std::vector<vk::Semaphore> &semaphores);

    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory deviceMemory;
    vk::DeviceSize size;

    std::shared_ptr<Device> device;
    const vk::UniqueCommandPool &commandPool;
    const vk::Queue &queue;
};


#endif//VULKANAPP_BUFFER_H
