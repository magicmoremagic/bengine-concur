#include "atex_app.hpp"
#include <be/core/log_exception.hpp>
#include <be/core/logging.hpp>
#include <be/util/paths.hpp>
#include <be/util/path_glob.hpp>
#include <be/util/parse_numeric_string.hpp>
#include <be/gfx/tex/visit_texture.hpp>
#include <be/gfx/tex/texture_reader.hpp>
#include <be/gfx/tex/duplicate_texture.hpp>
#include <be/gfx/tex/log_texture_info.hpp>
#include <be/gfx/tex/mipmapping.hpp>
#include <be/gfx/tex/blit_pixels.hpp>
#include <be/gfx/tex/betx_writer.hpp>
#include <map>

namespace be::atex {

using namespace be::gfx::tex;

///////////////////////////////////////////////////////////////////////////////
int AtexApp::operator()() {
   if (output_files_.empty()) {
      set_status_(status_no_output);
   }

   if (input_files_.empty()) {
      set_status_(status_no_input);
   }

   if (status_ != 0) {
      return status_;
   }

   try {
      if (input_search_paths_.empty()) {
         input_search_paths_.push_back(util::cwd());
      }

      if (output_path_base_.empty()) {
         output_path_base_ = util::cwd();
      }

      std::vector<input_> inputs = load_inputs_();
      if (inputs.empty()) {
         set_status_(status_no_input);
         return status_;
      }

      Texture tex = make_texture_(inputs);
      if (!tex.view) {
         set_status_(status_conversion_error);
         return status_;
      }

      log_texture_info(tex.view, "Texture Info");

      write_outputs_(tex.view);

   } catch (const FatalTrace& e) {
      set_status_(status_exception);
      log_exception(e);
   } catch (const RecoverableTrace& e) {
      set_status_(status_exception);
      log_exception(e);
   } catch (const fs::filesystem_error& e) {
      set_status_(status_exception);
      log_exception(e);
   } catch (const std::system_error& e) {
      set_status_(status_exception);
      log_exception(e);
   } catch (const std::exception& e) {
      set_status_(status_exception);
      log_exception(e);
   }

   return status_;
}

///////////////////////////////////////////////////////////////////////////////
void AtexApp::set_status_(status_code_ status) {
   if (status > status_) {
      status_ = static_cast<U8>(status);
   }
}

///////////////////////////////////////////////////////////////////////////////
std::vector<AtexApp::input_> AtexApp::load_inputs_() {
   std::vector<input_> inputs;
   std::map<std::size_t, std::size_t> images;

   for (input_file_ file : input_files_) {
      std::vector<Path> paths = util::glob(file.path.string(), input_search_paths_, util::PathMatchType::files_and_misc);
      if (paths.empty()) {
         set_status_(status_warning);
         be_short_warn() << "No files matched input file pattern: " << file.path.string() | default_log();
      } else {
         for (const Path& p : paths) {
            file.path = p;
            input_ input = load_input_(file);
            if (input.texture.view) {
               visit_texture_images(input.texture.view, [&](const ImageView& img) {

                  std::size_t layer = input.dest_layer + img.layer();
                  if (layer >= TextureStorage::max_layers) {
                     set_status_(status_warning);
                     be_warn() << "Too many layers; ignoring overflow!"
                        & attr("Source") << input.path.string()
                        & attr("Source Layer") << (file.first_layer + img.layer())
                        & attr("Dest Layer") << layer
                        | default_log();
                     return;
                  }

                  std::size_t face = input.dest_face + img.face();
                  if (face >= TextureStorage::max_faces) {
                     set_status_(status_warning);
                     be_warn() << "Too many faces; ignoring overflow!"
                        & attr("Source") << input.path.string()
                        & attr("Source Face") << (file.first_face + img.face())
                        & attr("Dest Face") << face
                        | default_log();
                     return;
                  }

                  std::size_t level = input.dest_level + img.level();
                  if (level >= TextureStorage::max_levels) {
                     set_status_(status_warning);
                     be_warn() << "Too many levels; ignoring overflow!"
                        & attr("Source") << input.path.string()
                        & attr("Source Level") << (file.first_level + img.level())
                        & attr("Dest Level") << level
                        | default_log();
                     return;
                  }

                  constexpr int layer_bits = 8 * sizeof(TextureStorage::layer_index_type);
                  constexpr int face_bits = 8 * sizeof(TextureStorage::face_index_type);
                  constexpr int level_bits = 8 * sizeof(TextureStorage::level_index_type);

                  std::size_t img_id = (layer << (face_bits + level_bits)) | (face << level_bits) | level;
                  auto result = images.insert(std::make_pair(img_id, inputs.size()));
                  if (!result.second) {
                     set_status_(status_warning);
                     be_warn() << "Replacing an image that was already loaded!"
                        & attr("Layer") << std::size_t(img.layer())
                        & attr("Face") << std::size_t(img.face())
                        & attr("Level") << std::size_t(img.level())
                        & attr("Old Source") << inputs[result.first->second].path.string()
                        & attr("New Source") << input.path.string()
                        | default_log();

                     result.first->second = inputs.size();
                  }
               });

               inputs.push_back(std::move(input));
            }
         }
      }
   }
   return inputs;
}

namespace {

///////////////////////////////////////////////////////////////////////////////
const std::regex& layer_regex() {
   static std::regex re("-(?:l|layer)(\\d+)", std::regex_constants::icase);
   return re;
}

///////////////////////////////////////////////////////////////////////////////
const std::regex& face_regex() {
   static std::regex re("-(?:f|face)(\\d+)", std::regex_constants::icase);
   return re;
}

///////////////////////////////////////////////////////////////////////////////
const std::regex& level_regex() {
   static std::regex re("-(?:m|level)(\\d+)", std::regex_constants::icase);
   return re;
}

} // be::atex::()

///////////////////////////////////////////////////////////////////////////////
AtexApp::input_ AtexApp::load_input_(const input_file_& file) {
   input_ result;
   result.path = file.path;
   result.dest_layer = file.layer;
   result.dest_face = file.face;
   result.dest_level = file.level;

   be_short_info() << "Loading " << file.file_format << " texture file: " << file.path.string() | default_log();

   if (file.first_layer > file.last_layer) {
      set_status_(status_warning);
      be_warn() << "No layers selected!"
         & attr(ids::log_attr_path) << file.path.string()
         & attr("First Layer") << file.first_layer
         & attr("Last Layer") << file.last_layer
         | default_log();
      return result;
   }

   if (file.first_face > file.last_face) {
      set_status_(status_warning);
      be_warn() << "No faces selected!"
         & attr(ids::log_attr_path) << file.path.string()
         & attr("First Face") << file.first_face
         & attr("Last Face") << file.last_face
         | default_log();
      return result;
   }

   if (file.first_level > file.last_level) {
      set_status_(status_warning);
      be_warn() << "No levels selected!"
         & attr(ids::log_attr_path) << file.path.string()
         & attr("First Level") << file.first_level
         & attr("Last Level") << file.last_level
         | default_log();
      return result;
   }

   if (result.dest_layer == TextureStorage::max_layers) {
      S filename = file.path.filename().generic_string();
      std::smatch m;
      if (std::regex_search(filename, m, layer_regex())) {
         S index = m[1].str();
         std::error_code ec;
         result.dest_layer = util::parse_bounded_numeric_string<input_file_::layer_index_type>(index, 0, TextureStorage::max_layers - 1, 10, ec);
         if (ec) {
            set_status_(status_warning);
            be_notice() << "Layer specified in filename is out of range; using layer 0 instead."
               & attr(ids::log_attr_path) << file.path.string()
               | default_log();
            result.dest_layer = 0;
         }
      } else {
         result.dest_layer = 0;
      }
   }

   if (result.dest_face == TextureStorage::max_faces) {
      S filename = file.path.filename().generic_string();
      std::smatch m;
      if (std::regex_search(filename, m, face_regex())) {
         S index = m[1].str();
         std::error_code ec;
         result.dest_face = util::parse_bounded_numeric_string<input_file_::face_index_type>(index, 0, TextureStorage::max_faces - 1, 10, ec);
         if (ec) {
            set_status_(status_warning);
            be_notice() << "Face specified in filename is out of range; using face 0 instead."
               & attr(ids::log_attr_path) << file.path.string()
               | default_log();
            result.dest_face = 0;
         }
      } else {
         result.dest_face = 0;
      }
   }

   if (result.dest_level == TextureStorage::max_levels) {
      S filename = file.path.filename().generic_string();
      std::smatch m;
      if (std::regex_search(filename, m, level_regex())) {
         S index = m[1].str();
         std::error_code ec;
         result.dest_level = util::parse_bounded_numeric_string<input_file_::level_index_type>(index, 0, TextureStorage::max_levels - 1, 10, ec);
         if (ec) {
            set_status_(status_warning);
            be_notice() << "Level specified in filename is out of range; using level 0 instead."
               & attr(ids::log_attr_path) << file.path.string()
               | default_log();
            result.dest_level = 0;
         }
      } else {
         result.dest_level = 0;
      }
   }

   TextureReader reader;
   if (file.file_format != TextureFileFormat::unknown) {
      reader.reset(file.file_format);
   }

   std::error_code ec;
   reader.read(file.path, ec);
   if (ec) {
      set_status_(status_read_error);
      log_exception(std::system_error(ec, "Failed to read texture file: " + file.path.string()));
   } else {
      result.texture = reader.texture(ec);
      if (ec) {
         set_status_(status_read_error);
         log_exception(std::system_error(ec, "Failed to parse texture file: " + file.path.string()));
      } else if (!result.texture.view) {
         set_status_(status_read_error);
         be_error() << "Loading texture file resulted in an empty texture!"
            & attr(ids::log_attr_path) << file.path.string()
            | default_log();
      } else {
         result.file_format = reader.format();
         TextureView& view = result.texture.view;
         log_texture_info(view, "Texture Loaded", result.path, result.file_format, v::verbose);

         ImageFormat new_format = view.format();

         if (file.override_colorspace) {
            new_format.colorspace(file.colorspace);
            be_short_verbose() << "Overriding colorspace: " << file.colorspace | default_log();
         }

         if (file.override_premultiplied) {
            new_format.premultiplied(file.premultiplied);
            be_short_verbose() << "Overriding premultiplied: " << (file.premultiplied ? "yes" : "no") | default_log();
         }

         if (file.override_components) {
            new_format.component_types(file.component_types);
            new_format.swizzles(file.swizzles);
            be_short_verbose() << "Overriding Component Type 0: " << new_format.component_type(0) | default_log();
            be_short_verbose() << "Overriding Component Type 1: " << new_format.component_type(1) | default_log();
            be_short_verbose() << "Overriding Component Type 2: " << new_format.component_type(2) | default_log();
            be_short_verbose() << "Overriding Component Type 3: " << new_format.component_type(3) | default_log();
            be_short_verbose() << "Overriding R Swizzle: " << new_format.swizzle(0) | default_log();
            be_short_verbose() << "Overriding G Swizzle: " << new_format.swizzle(1) | default_log();
            be_short_verbose() << "Overriding B Swizzle: " << new_format.swizzle(2) | default_log();
            be_short_verbose() << "Overriding A Swizzle: " << new_format.swizzle(3) | default_log();
         }

         TextureView new_view = TextureView(new_format, view.texture_class(), view.storage(),
                                            file.first_layer, file.last_layer - file.first_layer + 1,
                                            file.first_face, file.last_face - file.first_face + 1,
                                            file.first_level, file.last_level - file.first_level + 1);

         if (new_view.layers() != view.layers() || new_view.faces() != view.faces() || new_view.levels() != view.levels()) {
            if (new_view.layers() != view.layers()) {
               if (file.first_layer > 0) {
                  be_short_verbose() << "Skipping Layers: [ 0, " << std::size_t(file.first_layer - 1) << " ]" | default_log();
               }
               if (file.last_layer < view.layers() - 1) {
                  be_short_verbose() << "Skipping Layers: [ " << std::size_t(file.last_layer + 1) << ", " << std::size_t(view.layers() - 1) << " ]" | default_log();
               }
            }

            if (new_view.faces() != view.faces()) {
               if (file.first_face > 0) {
                  be_short_verbose() << "Skipping Faces: [ 0, " << std::size_t(file.first_face - 1) << " ]" | default_log();
               }
               if (file.last_face < view.faces() - 1) {
                  be_short_verbose() << "Skipping Faces: [ " << std::size_t(file.last_face + 1) << ", " << std::size_t(view.faces() - 1) << " ]" | default_log();
               }
            }

            if (new_view.levels() != view.levels()) {
               if (file.first_level > 0) {
                  be_short_verbose() << "Skipping Levels: [ 0, " << std::size_t(file.first_level - 1) << " ]" | default_log();
               }
               if (file.last_level < view.levels() - 1) {
                  be_short_verbose() << "Skipping Levels: [ " << std::size_t(file.last_level + 1) << ", " << std::size_t(view.levels() - 1) << " ]" | default_log();
               }
            }

            try {
               result.texture = duplicate_texture(new_view);
            } catch (const std::bad_alloc&) {
               view = TextureView();
               set_status_(status_read_error);
               log_exception(fs::filesystem_error("Not enough memory to duplicate texture", file.path, std::make_error_code(std::errc::not_enough_memory)));
            }
         } else {
            view = new_view;
         }
      }
   }

   return result;
}

///////////////////////////////////////////////////////////////////////////////
Texture AtexApp::make_texture_(const std::vector<input_>& inputs) {
   Texture result;

   be_verbose() << "Merging input textures" | default_log();

   std::map<std::size_t, std::pair<const input_*, ConstImageView>> images;
   TextureStorage::layer_index_type min_layer = TextureStorage::max_layers, max_layer = 0;
   TextureStorage::face_index_type min_face = TextureStorage::max_faces, max_face = 0;
   TextureStorage::level_index_type min_level = TextureStorage::max_levels, max_level = 0;
   const input_* base_input = nullptr;
   ivec3 base_dim;

   for (const auto& input : inputs) {
      ConstTextureView view = input.texture.view;
      visit_texture_images(view, [&](ConstImageView& img) {
         std::size_t layer = input.dest_layer + img.layer();
         std::size_t face = input.dest_face + img.face();
         std::size_t level = input.dest_level + img.level();
         if (layer >= TextureStorage::max_layers ||
             face >= TextureStorage::max_faces ||
             level >= TextureStorage::max_levels) {
            return;
         }

         constexpr int layer_bits = 8 * sizeof(TextureStorage::layer_index_type);
         constexpr int face_bits = 8 * sizeof(TextureStorage::face_index_type);
         constexpr int level_bits = 8 * sizeof(TextureStorage::level_index_type);

         std::size_t img_id = (layer << (face_bits + level_bits)) | (face << level_bits) | level;
         images[img_id] = std::make_pair(&input, img);

         if (level < min_level) {
            base_input = &input;
            base_dim = img.dim();
         }

         min_layer = std::min(min_layer, TextureStorage::layer_index_type(layer));
         max_layer = std::max(max_layer, TextureStorage::layer_index_type(layer));

         min_face = std::min(min_face, TextureStorage::face_index_type(face));
         max_face = std::max(max_face, TextureStorage::face_index_type(face));

         min_level = std::min(min_level, TextureStorage::level_index_type(level));
         max_level = std::max(max_level, TextureStorage::level_index_type(level));
      });
   }

   if (min_layer > 0) {
      set_status_(status_warning);
      be_short_warn() << "Missing layers: [ 0, " << std::size_t(min_layer - 1) << " ]" | default_log();
   }
   if (min_face > 0) {
      set_status_(status_warning);
      be_short_warn() << "Missing faces: [ 0, " << std::size_t(min_face - 1) << " ]" | default_log();
   }
   if (min_level > 0) {
      set_status_(status_warning);
      be_short_warn() << "Missing levels: [ 0, " << std::size_t(min_level - 1) << " ]" | default_log();

      for (glm::length_t n = 0; n < 3; ++n) {
         if (base_dim[n] > 1) {
            base_dim[n] <<= min_level;
         }
      }
   }

   TextureStorage::level_index_type expected_levels = mipmap_levels(base_dim);
   if (min_level + expected_levels <= max_level) {
      set_status_(status_warning);
      be_short_warn() << "Unnecessary mipmap levels removed: [ " << std::size_t(min_level + expected_levels) << ", " << std::size_t(max_level) << " ]" | default_log();
      max_level = min_level + expected_levels - 1;
   }

   for (TextureStorage::layer_index_type layer = min_layer; layer <= max_layer; ++layer) {
      for (TextureStorage::face_index_type face = min_face; face <= max_face; ++face) {
         for (TextureStorage::level_index_type level = min_level; level <= max_level; ++level) {
            constexpr int layer_bits = 8 * sizeof(TextureStorage::layer_index_type);
            constexpr int face_bits = 8 * sizeof(TextureStorage::face_index_type);
            constexpr int level_bits = 8 * sizeof(TextureStorage::level_index_type);

            std::size_t img_id = (layer << (face_bits + level_bits)) | (face << level_bits) | level;
            auto it = images.find(img_id);
            if (it == images.end()) {
               set_status_(status_warning);
               be_short_warn() << "Missing image for layer " << std::size_t(layer) << " face " << std::size_t(face) << " level " << std::size_t(level) | default_log();
            } else {
               auto dim = it->second.second.dim();
               auto expected = mipmap_dim(base_dim, level);
               if (dim != expected) {
                  set_status_(status_warning);
                  be_warn() << "Image size mismatch!"
                     & attr("Source Path") << it->second.first->path.string()
                     & attr("Width") << dim.x
                     & attr("Expected Width") << expected.x
                     & attr("Height") << dim.y
                     & attr("Expected Height") << expected.y
                     & attr("Depth") << dim.z
                     & attr("Expected Depth") << expected.z
                     & attr("Destination Layer") << std::size_t(layer)
                     & attr("Destination Face") << std::size_t(face)
                     & attr("Destination Level") << std::size_t(level)
                     | default_log();
               }
            }
         }
      }
   }

   assert(base_input != nullptr);

   TextureStorage::layer_index_type layers = max_layer + 1;
   TextureStorage::face_index_type faces = max_face + 1;
   TextureStorage::level_index_type levels = max_level + 1;

   TextureClass tex_class;
   if (override_tex_class_) {
      tex_class = tex_class_;
   } else {
      tex_class = base_input->texture.view.texture_class();
      if (layers > 1 && !is_array(tex_class)) {
         switch (tex_class) {
            case TextureClass::lineal: tex_class = TextureClass::lineal_array; break;
            case TextureClass::planar: tex_class = TextureClass::planar_array; break;
            case TextureClass::volumetric: tex_class = TextureClass::volumetric_array; break;
            case TextureClass::directional: tex_class = TextureClass::directional_array; break;
            default: break;
         }
      }
   }

   if (layers > 1 && !is_array(tex_class)) {
      set_status_(status_warning);
      be_notice() << "Using non-array texture class for a texture with multiple layers"
         & attr("Texture Class") << tex_class
         & attr("Layers") << std::size_t(layers)
         | default_log();
   }

   if (faces != gfx::tex::faces(tex_class)) {
      set_status_(status_warning);
      be_notice() << "Face count conflict"
         & attr("Texture Class") << tex_class
         & attr("Faces") << std::size_t(faces)
         & attr("Expected Faces") << std::size_t(gfx::tex::faces(tex_class))
         | default_log();
   }

   if (base_dim.z > 1 && dimensionality(tex_class) < 3 ||
       base_dim.y > 1 && dimensionality(tex_class) < 2) {
      set_status_(status_warning);
      be_notice() << "Texture class dimensionality conflict"
         & attr("Texture Class") << tex_class
         & attr("Dimensionality") << std::size_t(dimensionality(tex_class))
         & attr("Width") << base_dim.x
         & attr("Height") << base_dim.y
         & attr("Depth") << base_dim.z
         | default_log();
   }

   ImageFormat format = base_input->texture.view.format();
   U8 block_span = base_input->texture.view.block_span();
   if (override_block_) {
      format.packing(packing_);
      format.block_dim(ImageFormat::block_dim_type(1));
      format.block_size(block_word_size(packing_) * block_word_count(packing_));
      format.components(components_);
      format.component_types(component_types_);
      format.swizzles(swizzles_);
      block_span = block_span_;
   }

   if (format.components() > component_count(format.packing())) {
      set_status_(status_warning);
      be_notice() << "Component count conflict"
         & attr("Block Packing") << format.packing()
         & attr("Components") << std::size_t(format.components())
         & attr("Expected Components") << std::size_t(component_count(format.packing()))
         | default_log();
   }

   std::size_t required_block_size = format.block_dim().x * format.block_dim().y * format.block_dim().z *
      block_word_size(format.packing()) * block_word_count(format.packing());

   if (required_block_size > format.block_size()) {
      set_status_(status_warning);
      be_notice() << "Block size enlarged to fit all block data"
         & attr("Block Packing") << format.packing()
         & attr("Block Width") << std::size_t(format.block_dim().x)
         & attr("Block Height") << std::size_t(format.block_dim().y)
         & attr("Block Depth") << std::size_t(format.block_dim().z)
         & attr("Block Size") << std::size_t(format.block_size())
         & attr("Required Block Size") << required_block_size
         | default_log();
      format.block_size(ImageFormat::block_size_type(required_block_size));
   }

   if (block_span < format.block_size()) {
      set_status_(status_warning);
      be_notice() << "Block span increased to fit all block data"
         & attr("Block Span") << std::size_t(block_span)
         & attr("Required Block Span") << std::size_t(format.block_size())
         | default_log();
      block_span = format.block_size();
   }

   if (override_colorspace_) {
      format.colorspace(colorspace_);
   }

   if (override_premultiplied_) {
      format.premultiplied(premultiplied_);
   }

   TextureAlignment alignment;
   if (override_alignment_) {
      alignment = TextureAlignment(line_alignment_bits_, plane_alignment_bits_, level_alignment_bits_, face_alignment_bits_, layer_alignment_bits_);
   } else {
      alignment = base_input->texture.view.storage().alignment();
   }

   try {
      result.storage = std::make_unique<TextureStorage>(layers, faces, levels, base_dim, format.block_dim(), block_span, alignment);
   } catch (const std::bad_alloc&) {
      set_status_(status_conversion_error);
      log_exception(std::system_error(std::make_error_code(std::errc::not_enough_memory), "Not enough memory to allocate merged texture"));
      return result;
   }

   result.view = TextureView(format, tex_class, *result.storage, 0, layers, 0, faces, 0, levels);

   visit_texture_images(result.view, [&](ImageView& img) {
      std::size_t layer = img.layer();
      std::size_t face = img.face();
      std::size_t level = img.level();

      constexpr int layer_bits = 8 * sizeof(TextureStorage::layer_index_type);
      constexpr int face_bits = 8 * sizeof(TextureStorage::face_index_type);
      constexpr int level_bits = 8 * sizeof(TextureStorage::level_index_type);

      std::size_t img_id = (layer << (face_bits + level_bits)) | (face << level_bits) | level;

      auto it = images.find(img_id);
      if (it != images.end()) {
         // TODO make sure we can do the conversion (eg. not converting to compressed)

         ConstImageView src = it->second.second;
         ImageRegion region = ImageRegion(pixel_region(src).extents().intersection(pixel_region(img).extents()));
         blit_pixels(src, region, img, region);
      }
   });

   return result;
}

///////////////////////////////////////////////////////////////////////////////
void AtexApp::write_outputs_(TextureView view) {
   for (output_file_ file : output_files_) {
      file.path = fs::absolute(file.path, output_path_base_);

      if (fs::exists(file.path) && !overwrite_output_files_) {
         set_status_(status_write_error);
         be_error() << "Skipping ouput file: file already exists; use --overwrite to ignore."
            & attr(ids::log_attr_output_path) << file.path.string()
            | default_log();
         continue;
      }

      S filename = file.path.filename().generic_string();

      if (!file.force_layers) {
         std::smatch m;
         if (std::regex_search(filename, m, layer_regex())) {
            S index = m[1].str();
            std::error_code ec;
            file.base_layer = util::parse_bounded_numeric_string<input_file_::layer_index_type>(index, 0, TextureStorage::max_layers - 1, 10, ec);
            file.layers = 1;
            if (ec) {
               set_status_(status_write_error);
               log_exception(fs::filesystem_error("Invalid layer specified in output filename.", file.path, ec));
               continue;
            }
         }
      }

      if (!file.force_faces) {
         std::smatch m;
         if (std::regex_search(filename, m, face_regex())) {
            S index = m[1].str();
            std::error_code ec;
            file.base_face = util::parse_bounded_numeric_string<input_file_::face_index_type>(index, 0, TextureStorage::max_faces - 1, 10, ec);
            file.faces = 1;
            if (ec) {
               set_status_(status_write_error);
               log_exception(fs::filesystem_error("Invalid face specified in output filename.", file.path, ec));
               continue;
            }
         }
      }

      if (!file.force_levels) {
         std::smatch m;
         if (std::regex_search(filename, m, level_regex())) {
            S index = m[1].str();
            std::error_code ec;
            file.base_level = util::parse_bounded_numeric_string<input_file_::level_index_type>(index, 0, TextureStorage::max_levels - 1, 10, ec);
            file.levels = 1;
            if (ec) {
               set_status_(status_write_error);
               log_exception(fs::filesystem_error("Invalid mipmap level specified in output filename.", file.path, ec));
               continue;
            }
         }
      }

      if (file.base_layer >= view.layers()) {
         set_status_(status_write_error);
         be_error() << "Skipping ouput file: no layers selected!"
            & attr(ids::log_attr_output_path) << file.path.string()
            | default_log();
         continue;
      }

      if (file.base_face >= view.faces()) {
         set_status_(status_write_error);
         be_error() << "Skipping ouput file: no faces selected!"
            & attr(ids::log_attr_output_path) << file.path.string()
            | default_log();
         continue;
      }

      if (file.base_level >= view.levels()) {
         set_status_(status_write_error);
         be_error() << "Skipping ouput file: no levels selected!"
            & attr(ids::log_attr_output_path) << file.path.string()
            | default_log();
         continue;
      }

      TextureView selected_view = TextureView(view.format(), view.texture_class(), view.storage(),
                                              view.base_layer() + file.base_layer, file.layers,
                                              view.base_face() + file.base_face, file.faces,
                                              view.base_level() + file.base_level, file.levels);

      if (file.file_format == TextureFileFormat::unknown) {
         S ext = file.path.extension().generic_string();
         std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return (char)tolower(c); });
         if (".betx" == ext) {
            file.file_format = TextureFileFormat::betx;
         } else if (".ktx" == ext) {
            file.file_format = TextureFileFormat::ktx;
         } else if (".dds" == ext) {
            file.file_format = TextureFileFormat::dds;
         } else if (".png" == ext) {
            file.file_format = TextureFileFormat::png;
         } else if (".tga" == ext) {
            file.file_format = TextureFileFormat::tga;
         } else if (".bmp" == ext || ".dib" == ext) {
            file.file_format = TextureFileFormat::bmp;
         } else if (".hdr" == ext || ".rgbe" == ext || ".pic" == ext) {
            // .pic is used for both radiance and softimage.  Since we can't write softimage we'll assume it means radiance here
            file.file_format = TextureFileFormat::hdr;
         }
      }

      switch (file.file_format) {
         case TextureFileFormat::unknown:
            set_status_(status_write_error);
            be_error() << "Could not determine output texture file format!"
               & attr(ids::log_attr_output_path) << file.path.string()
               | default_log();
            break;

         case TextureFileFormat::betx:
         case TextureFileFormat::ktx:
         case TextureFileFormat::dds:
            write_output_(selected_view, file.path, file.file_format, file.byte_order, file.payload_compression);
            break;

         default:
            // image files don't support multiple layers/faces/levels
            write_layer_images_(selected_view, file);
            break;
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void AtexApp::write_layer_images_(TextureView view, output_file_ file) {

   if (view.layers() <= 1) {
      write_face_images_(view, std::move(file));
   } else {
      Path parent_path = file.path.parent_path();
      S base = file.path.stem().string() + "-layer";
      S ext = file.path.extension().string();

      for (TextureStorage::layer_index_type layer = TextureStorage::layer_index_type(view.base_layer());
           layer < view.base_layer() + view.layers(); ++layer) {
         file.path = parent_path / Path(base + std::to_string((std::size_t)layer) + ext);
         TextureView layer_view = TextureView(view.format(), view.texture_class(), view.storage(),
                                              layer, 1,
                                              view.base_face(), view.faces(),
                                              view.base_level(), view.levels());
         write_face_images_(layer_view, file);
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void AtexApp::write_face_images_(TextureView view, output_file_ file) {
   if (view.faces() <= 1) {
      write_level_images_(view, std::move(file));
   } else {
      Path parent_path = file.path.parent_path();
      S base = file.path.stem().string() + "-face";
      S ext = file.path.extension().string();

      for (TextureStorage::face_index_type face = TextureStorage::face_index_type(view.base_face());
           face < view.base_face() + view.faces(); ++face) {
         file.path = parent_path / Path(base + std::to_string((std::size_t)face) + ext);
         TextureView face_view = TextureView(view.format(), view.texture_class(), view.storage(),
                                             view.base_layer(), view.layers(),
                                             face, 1,
                                             view.base_level(), view.levels());
         write_level_images_(face_view, file);
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void AtexApp::write_level_images_(TextureView view, output_file_ file) {
   if (view.levels() <= 1) {
      write_output_(view, file.path, file.file_format, file.byte_order, file.payload_compression);
   } else {
      Path parent_path = file.path.parent_path();
      S base = file.path.stem().string() + "-level";
      S ext = file.path.extension().string();

      for (TextureStorage::level_index_type level = TextureStorage::level_index_type(view.base_level());
           level < view.base_level() + view.levels(); ++level) {
         file.path = parent_path / Path(base + std::to_string((std::size_t)level) + ext);
         TextureView level_view = TextureView(view.format(), view.texture_class(), view.storage(),
                                              view.base_layer(), view.layers(),
                                              view.base_face(), view.faces(),
                                              level, 1);
         write_output_(level_view, file.path, file.file_format, file.byte_order, file.payload_compression);
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void AtexApp::write_output_(TextureView view, const Path& path, TextureFileFormat format, ByteOrderType byte_order, bool payload_compression) {
   std::error_code ec;

   be_short_info() << "Writing " << format << " texture file: " << path.string() | default_log();

   switch (format) {
      case TextureFileFormat::betx:
      {
         BetxWriter writer;
         writer.payload_compression(payload_compression ? BetxWriter::PayloadCompressionMode::zlib : BetxWriter::PayloadCompressionMode::none);
         writer.endianness(byte_order);
         writer.texture(view);
         writer.write(path, ec);
         break;
      }
      case TextureFileFormat::ktx:
         // TODO
      case TextureFileFormat::dds:
         // TODO
      case TextureFileFormat::png:
         // TODO
      case TextureFileFormat::tga:
         // TODO
      case TextureFileFormat::bmp:
         // TODO
      case TextureFileFormat::hdr:
         // TODO
      default:
         ec = std::make_error_code(std::errc::not_supported);
         break;
   }
   if (ec) {
      set_status_(status_write_error);
      log_exception(fs::filesystem_error("Could not write output texture!", path, ec));
   }
}

} // be::atex
