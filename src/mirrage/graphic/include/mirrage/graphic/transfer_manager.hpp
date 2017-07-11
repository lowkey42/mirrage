#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/ring_buffer.hpp>

#include <vulkan/vulkan.hpp>
#include <gsl/gsl>

#include <memory>
#include <cstdint>
#include <tuple>


namespace mirrage {
namespace graphic {

	class Device;


	// TODO: wrapper classes (Static_buffer/Static_image) are not realy required anymore and we
	//        'could' just return Backed_buffer/Backed_image
	class Static_buffer {
		public:
			Static_buffer(Backed_buffer buffer)
			    : _buffer(std::move(buffer)) {}
			Static_buffer(Static_buffer&&)noexcept ;
			Static_buffer& operator=(Static_buffer&&)noexcept;

			auto buffer()const noexcept {return *_buffer;}

		private:
			Backed_buffer _buffer;
	};
	class Static_image {
		public:
			Static_image(Backed_image image, std::uint32_t mip_count, bool generate_mips,
			             Image_dimensions dimensions)
			    : _image(std::move(image)), _mip_count(mip_count), _generate_mips(generate_mips)
			    , _dimensions(dimensions) {}
			Static_image(Static_image&&)noexcept;

			auto image()const noexcept {return *_image;}

			auto mip_level_count()const noexcept {return _mip_count;}
			auto width()const noexcept {return _dimensions.width;}
			auto height()const noexcept {return _dimensions.height;}
			auto depth()const noexcept {return _dimensions.depth;}
			auto layers()const noexcept {return _dimensions.layers;}

		private:
			Backed_image  _image;
			std::uint32_t _mip_count;
			bool          _generate_mips;
			Image_dimensions _dimensions;
	};

	class Dynamic_buffer {
		public:
			Dynamic_buffer(Backed_buffer buffer, std::size_t capacity,
			               vk::PipelineStageFlags earliest_usage, vk::AccessFlags earliest_usage_access,
			               vk::PipelineStageFlags latest_usage, vk::AccessFlags latest_usage_access)
			    : _buffer(std::move(buffer)), _capacity(capacity)
			    , _earliest_usage(earliest_usage), _earliest_usage_access(earliest_usage_access)
			    , _latest_usage(latest_usage), _latest_usage_access(latest_usage_access) {}

			template<class T>
			void update_obj(const Command_buffer& cb, const T& obj) {
				static_assert(std::is_standard_layout<T>::value, "");
				update(cb, 0, gsl::span<const char>(reinterpret_cast<const char*>(&obj), sizeof(T)));
			}
			template<class T>
			void update_objs(const Command_buffer& cb, gsl::span<T> obj) {
				static_assert(std::is_standard_layout<T>::value, "");
				update(cb, 0, gsl::span<const char>(reinterpret_cast<const char*>(obj.data()),
				                                    obj.size_bytes()));
			}

			void update(const Command_buffer& cb, vk::DeviceSize dstOffset,
			            gsl::span<const char> data);
			
			/// F = void(void(tuple<vk::DiviceOffset, gsl::span<const char>))
			template<class F>
			void update_bulk(const Command_buffer& cb, F&& f) {
				_pre_update(cb);
				f([&](auto offset, auto&& data) {
					_do_update(cb, offset, std::forward<decltype(data)>(data));
				});
				_post_update(cb);
			}

			auto buffer()const noexcept {return *_buffer;}
			auto capacity()const noexcept {return _capacity;}

		private:
			Backed_buffer  _buffer;
			const std::size_t _capacity;
			vk::PipelineStageFlags _earliest_usage;
			vk::AccessFlags        _earliest_usage_access;
			vk::PipelineStageFlags _latest_usage;
			vk::AccessFlags        _latest_usage_access;
			
			void _pre_update(const Command_buffer& cb);
			void _do_update(const Command_buffer& cb, vk::DeviceSize dstOffset,
			                gsl::span<const char> data);
			void _post_update(const Command_buffer& cb);
	};


	class Transfer_manager {
		public:
			Transfer_manager(Device& device, Queue_tag transfer_queue, std::size_t max_frames=6);
			~Transfer_manager();

			/**
			 * Creates a new image based on the given properties and uploads the pixel data inplace.
			 * write_data is called exacly once and is expected to write $size bytes into the buffer
			 * beginning at the given address.
			 *
			 * The data should match this subsection of the format specified
			 *   by https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
			 * The padding at the end of the data block may be ommited.
			 * The cubePadding is expected to be ALWAYS 0 bytes long!
			 *
			 *  for each mipmap_level in numberOfMipmapLevels*
			 *      UInt32 imageSize;
			 *      for each array_element in numberOfArrayElements*
			 *         for each face in numberOfFaces
			 *             for each z_slice in pixelDepth*
			 *                 for each row or row_of_blocks in pixelHeight*
			 *                     for each pixel or block_of_pixels in pixelWidth
			 *                         Byte data[format-specific-number-of-bytes]**
			 *                     end
			 *                 end
			 *             end
			 *             Byte cubePadding[0-3]
			 *         end
			 *      end
			 *      Byte mipPadding[3 - ((imageSize + 3) % 4)]
			 *  end
			 *
			 */
			auto upload_image(vk::ImageType, std::uint32_t owner, const Image_dimensions&, vk::Format,
			                  std::uint32_t mip_levels, std::uint32_t size,
			                  std::function<void(char*)> write_data,
			                  bool dedicated=false) -> Static_image;

			auto upload_buffer(vk::BufferUsageFlags usage, std::uint32_t owner,
			                   gsl::span<const char> data,
			                   bool dedicated=false) -> Static_buffer {
				return upload_buffer(usage, owner, std::initializer_list<gsl::span<const char>>{data},
				                     dedicated);
			}

			auto upload_buffer(vk::BufferUsageFlags usage, std::uint32_t owner,
			                   std::initializer_list<gsl::span<const char>> data,
			                   bool dedicated=false) -> Static_buffer {
				auto size = std::uint32_t(0);
				for(auto&& subdata : data) {
					size += subdata.size_bytes();
				}

				return upload_buffer(usage, owner, size, [&](char* dest) {
					auto offset = std::ptrdiff_t(0);

					for(auto&& subdata : data) {
						auto subdata_size = subdata.size_bytes();
						std::memcpy(dest+offset, subdata.data(), subdata_size);
						offset += subdata_size;
					}
				}, dedicated);
			}

			auto upload_buffer(vk::BufferUsageFlags usage, std::uint32_t owner,
			                   std::uint32_t size, std::function<void(char*)> write_data,
			                   bool dedicated=false) -> Static_buffer;

			auto create_dynamic_buffer(vk::DeviceSize size, vk::BufferUsageFlags,
			                           vk::PipelineStageFlags earliest_usage,
			                           vk::AccessFlags earliest_usage_access,
			                           vk::PipelineStageFlags latest_usage,
			                           vk::AccessFlags latest_usage_access) -> Dynamic_buffer;

			/**
			 * @brief has to be called exacly once per frame, executes the transfer operations
			 * @arg The command buffer that will be filled with all required barriers (should be
			 *        submitted before anything else in this frame)
			 * @return the semaphore that all draw operations have to wait on
			 */
			auto next_frame(vk::CommandBuffer) -> util::maybe<vk::Semaphore>;

		private:
			struct Transfer_buffer_req {
				Backed_buffer src;
				vk::Buffer dst;
				vk::DeviceSize size;
				std::uint32_t owner;

				Transfer_buffer_req() = default;
				Transfer_buffer_req(Backed_buffer src, vk::Buffer dst, vk::DeviceSize size,
				                    std::uint32_t owner)
				    : src(std::move(src)), dst(dst), size(size), owner(owner) {}
			};
			struct Transfer_image_req {
				Backed_buffer src;
				vk::Image dst;
				vk::DeviceSize size;
				std::uint32_t owner;
				std::uint32_t mip_count_actual;
				std::uint32_t mip_count_loaded;
				bool generate_mips;
				Image_dimensions dimensions;
				std::vector<std::uint32_t> mip_image_sizes; //< analog to imageSize in KTX format

				Transfer_image_req() = default;
				Transfer_image_req(Backed_buffer src, vk::Image dst, vk::DeviceSize size,
				                   std::uint32_t owner, std::uint32_t mip_count_actual,
				                   std::uint32_t mip_count_loaded, bool generate_mips,
				                   Image_dimensions dimensions,
				                   std::vector<std::uint32_t> mip_image_sizes)
			       : src(std::move(src)), dst(dst), size(size), owner(owner)
				   , mip_count_actual(mip_count_actual), mip_count_loaded(mip_count_loaded)
			       , generate_mips(generate_mips), dimensions(dimensions)
				   , mip_image_sizes(std::move(mip_image_sizes)) {}
			};

			Device& _device;
			std::uint32_t _queue_family;
			vk::Queue _queue;
			vk::UniqueSemaphore _semaphore;
			Command_buffer_pool _command_buffer_pool;

			Per_frame_queue<vk::UniqueCommandBuffer> _command_buffers;

			std::vector<Transfer_buffer_req> _buffer_transfers;
			std::vector<Transfer_image_req>  _image_transfers;

			auto _get_next_fence() -> vk::Fence;

			auto _create_staging_buffer(vk::BufferUsageFlags, Memory_lifetime,
			                            std::uint32_t size,
			                            std::function<void(char*)> write_data) -> Backed_buffer;
			void _transfer_image(vk::CommandBuffer cb, const Transfer_image_req&);
	};

}
}
