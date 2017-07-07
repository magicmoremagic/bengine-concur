#pragma once
#ifndef BE_ATEX_ATEX_APP_HPP_
#define BE_ATEX_ATEX_APP_HPP_

#include <be/core/lifecycle.hpp>
#include <be/core/filesystem.hpp>
#include <be/gfx/tex/texture.hpp>
#include <be/gfx/tex/texture_file_format.hpp>
#include <be/core/glm.hpp>
#include <be/core/byte_order.hpp>

namespace be::atex {

///////////////////////////////////////////////////////////////////////////////
class AtexApp final {
public:
   AtexApp(int argc, char** argv);

   int operator()();

private:
   enum status_code_ : U8 {
      status_ok = 0,
      status_warning,
      status_exception,
      status_cli_error,
      status_no_output,
      status_no_input,
      status_read_error,
      status_conversion_error,
      status_write_error
   };

   struct input_file_ {
      using layer_index_type = gfx::tex::TextureStorage::layer_index_type;
      using face_index_type = gfx::tex::TextureStorage::face_index_type;
      using level_index_type = gfx::tex::TextureStorage::level_index_type;

      Path path;
      gfx::tex::TextureFileFormat file_format;

      layer_index_type layer = gfx::tex::TextureStorage::max_layers;
      layer_index_type first_layer = 0;
      layer_index_type last_layer = gfx::tex::TextureStorage::max_layers - 1;

      face_index_type face = gfx::tex::TextureStorage::max_faces;
      face_index_type first_face = 0;
      face_index_type last_face = gfx::tex::TextureStorage::max_faces - 1;

      level_index_type level = gfx::tex::TextureStorage::max_levels;
      level_index_type first_level = 0;
      level_index_type last_level = gfx::tex::TextureStorage::max_levels - 1;

      bool override_components = false;
      gfx::tex::ImageFormat::component_types_type component_types;
      gfx::tex::ImageFormat::swizzles_type swizzles = gfx::tex::swizzles_rgba();

      bool override_colorspace = false;
      gfx::tex::Colorspace colorspace = gfx::tex::Colorspace::unknown;

      bool override_premultiplied = false;
      bool premultiplied = false;
   };
   struct input_ {
      Path path;
      gfx::tex::TextureFileFormat file_format = gfx::tex::TextureFileFormat::unknown;
      gfx::tex::Texture texture;
      gfx::tex::TextureStorage::layer_index_type dest_layer = gfx::tex::TextureStorage::max_layers;
      gfx::tex::TextureStorage::face_index_type dest_face = gfx::tex::TextureStorage::max_faces;
      gfx::tex::TextureStorage::level_index_type dest_level = gfx::tex::TextureStorage::max_levels;
   };

   struct output_file_ {
      using layer_index_type = gfx::tex::TextureStorage::layer_index_type;
      using face_index_type = gfx::tex::TextureStorage::face_index_type;
      using level_index_type = gfx::tex::TextureStorage::level_index_type;

      Path path;
      gfx::tex::TextureFileFormat file_format;

      bool force_layers = false;
      layer_index_type base_layer = 0;
      layer_index_type layers = gfx::tex::TextureStorage::max_layers;

      bool force_faces = false;
      face_index_type base_face = 0;
      face_index_type faces = gfx::tex::TextureStorage::max_faces;

      bool force_levels = false;
      level_index_type base_level = 0;
      level_index_type levels = gfx::tex::TextureStorage::max_levels;

      ByteOrderType byte_order = bo::Host::value;
      bool payload_compression = false;
   };

   void set_status_(status_code_ status);

   std::vector<input_> load_inputs_();
   input_ load_input_(const input_file_& file);
   gfx::tex::Texture make_texture_(const std::vector<input_>& inputs);
   void write_outputs_(gfx::tex::TextureView view);
   void write_layer_images_(gfx::tex::TextureView view, output_file_ file);
   void write_face_images_(gfx::tex::TextureView view, output_file_ file);
   void write_level_images_(gfx::tex::TextureView view, output_file_ file);
   void write_output_(gfx::tex::TextureView view, const Path& path, gfx::tex::TextureFileFormat format, ByteOrderType byte_order, bool payload_compression);

   CoreInitLifecycle init_;
   I8 status_ = 0;

   std::vector<Path> input_search_paths_;
   std::vector<input_file_> input_files_;
   
   bool override_block_ = false;
   gfx::tex::BlockPacking packing_ = gfx::tex::BlockPacking::s_8_8_8_8;
   U8 components_ = 4;
   gfx::tex::ImageFormat::component_types_type component_types_;
   gfx::tex::ImageFormat::swizzles_type swizzles_ = gfx::tex::swizzles_rgba();
   U8 block_span_ = 0;

   bool override_colorspace_ = false;
   gfx::tex::Colorspace colorspace_ = gfx::tex::Colorspace::srgb;

   bool override_premultiplied_ = false;
   bool premultiplied_ = false;

   bool override_alignment_ = false;
   U8 line_alignment_bits_ = 2u;
   U8 plane_alignment_bits_ = 0;
   U8 level_alignment_bits_ = 0;
   U8 face_alignment_bits_ = 0;
   U8 layer_alignment_bits_ = 0;

   bool override_tex_class_ = false;
   gfx::tex::TextureClass tex_class_;

   Path output_path_base_;
   std::vector<output_file_> output_files_;
   bool overwrite_output_files_ = false;
};

} // be::atex

#endif
