#include "atex_app.hpp"
#include "version.hpp"
#include <be/gfx/version.hpp>
#include <be/core/version.hpp>
#include <be/core/log_exception.hpp>
#include <be/cli/cli.hpp>
#include <be/util/paths.hpp>
#include <iostream>
#include <regex>

namespace be::atex {

///////////////////////////////////////////////////////////////////////////////
AtexApp::AtexApp(int argc, char** argv) {
   default_log().verbosity_mask(v::info_or_worse);
   try {
      using namespace cli;
      using namespace color;
      using namespace ct;
      using namespace gfx::tex;
      Processor proc;

      bool show_version = false;
      bool show_help = false;
      bool verbose = false;
      S help_query;

      bool configure_output = false;
      auto configuring_input = [&]() { return !configure_output; };
      auto configuring_output = [&]() { return configure_output; };

      TextureFileFormat default_input_format = TextureFileFormat::unknown;
      TextureFileFormat default_output_format = TextureFileFormat::unknown;

      input_file_ next_input;
      output_file_ next_output;

#pragma region cli::Processor decls
      proc
         (prologue (Table() << header << "TEXTURE ASSEMBLY TOOL").query())

         (synopsis (Cell() << fg_dark_gray << "{ " << fg_cyan << "OPTIONS" << fg_blue << " INPUT" << fg_dark_gray << " } ["
                           << fg_yellow << "--" << fg_dark_gray << " { " << fg_cyan << "OPTIONS" << fg_blue << " OUTPUT" << fg_dark_gray << " } ]" ))

         (abstract ("Converts, combines, and extracts images and textures."))

         (summary ("Execution consists of two phases.  First, one or more input images or textures are loaded.  In the second phase, each input image/texture is "
                   "copied into a single in-memory texture, converting the texel format if necessary.  Then one or more image or texture views are written to disk.").verbose())

         (summary ("Although texel format, colorspace, alpha premultiplication, and channel swizzling conversions can be performed on textures, no other operations will be performed, including "
                   "rescaling, cropping, mipmap generation, rotation, distortion, compositing, exposure/color correction, etc.  Compressed texel formats can be converted to uncompressed texel "
                   "formats, but compressed texel formats can only be output if the input textures are provided in the exact same compressed texel format and no colorspace or alpha "
                   "premultiplication conversions are required.").verbose())

         (summary (Cell() << "If any input texture field types or swizzles are reinterpreted with " << fg_yellow << "--ctype-*" << reset << " or " << fg_yellow
                          << "--swizzle-*" << reset << " then they are all reinterpreted.  Field types will default to " << fg_cyan << "none" << reset
                          << " and swizzles will default to RGBA.").verbose())

         (summary (Cell() << "The texel format used for output textures will be the same as the first input texture for the lowest output mipmap level.  If " << fg_yellow << "--packing " << reset
                          << "is specified, the block packing, component count, field types, swizzles, and block span are all overridden.  Otherwise the options which control those aspects of the "
                             "texel format will be ignored.  If any of the " << fg_yellow << "--*-align" << reset << " options are used, all other alignment parameters "
                             "will also be overridden.  Alignment is specified as a base-2 exponent; the actual alignment is (1 << " << fg_cyan << "BITS" << reset << ").").verbose())

         (summary (Cell() << "Supported input texture file types: " << fg_green << "beTx"
                          << fg_dark_gray << ", " << fg_green << "DDS"
                          << fg_dark_gray << ", " << fg_green << "KTX").verbose())
         (summary (Cell() << "Supported input image file types: " << fg_green << "glRaw"
                          << fg_dark_gray << ", " << fg_green << "PNG"
                          << fg_dark_gray << ", " << fg_green << "Targa"
                          << fg_dark_gray << ", " << fg_green << "Radiance RGBE"
                          << fg_dark_gray << ", " << fg_green << "PPM"
                          << fg_dark_gray << ", " << fg_green << "PBM"
                          << fg_dark_gray << ", " << fg_green << "Softimage PIC"
                          << fg_dark_gray << ", " << fg_green << "DIB"
                          << fg_dark_gray << ", " << fg_green << "JPEG"
                          << fg_dark_gray << ", " << fg_green << "GIF").verbose())

         (summary (Cell() << "Supported output texture file types: " << fg_green << "beTx"
                          << fg_dark_gray << ", " << fg_green << "DDS"
                          << fg_dark_gray << ", " << fg_green << "KTX").verbose())
         (summary (Cell() << "Supported output image file types: " << fg_green << "PNG"
                          << fg_dark_gray << ", " << fg_green << "Targa"
                          << fg_dark_gray << ", " << fg_green << "Radiance RGBE"
                          << fg_dark_gray << ", " << fg_green << "DIB").verbose())

         (doc (ids::cli_describe_section_options_compact, Cell() << fg_gray << "INPUT OPTIONS"))
         (doc (ids::cli_describe_section_options_manstyle, Cell() << fg_gray << "INPUT OPTIONS"))
         (doc (ids::cli_describe_section_options_manstyle, ""))

         (numeric_param<TextureStorage::layer_index_type> ({ "l" }, { "layer" }, "N", 0, TextureStorage::max_layers - 1, [&](TextureStorage::layer_index_type layer) {
               next_input.layer = layer;
            }).when(configuring_input)
              .desc("The first selected layer in the next input texture will be copied to this layer in the in-memory texture.")
              .extra(Cell() << "If not specified, and part of the filename matches" << fg_green << " /-(l|layer)\\d+/ " << reset
                            << "then that index will be used, otherwise defaults to 0.  If multiple layers are selected from the next input texture, "
                               "they will be copied to subsequent layers."))

         (numeric_param<TextureStorage::face_index_type> ({ "f" }, { "face" }, "N", 0, TextureStorage::max_faces - 1, [&](TextureStorage::face_index_type face) {
               next_input.face = face;
            }).when(configuring_input)
              .desc("The first selected face in the next input texture will be copied to this face in the in-memory texture.")
              .extra(Cell() << "If not specified, and part of the filename matches" << fg_green << " /-(f|face)\\d+/ " << reset
                            << "then that index will be used, otherwise defaults to 0.  If multiple faces are selected from the next input texture, "
                               "they will be copied to subsequent faces."))

         (numeric_param<TextureStorage::level_index_type> ({ "m" }, { "level" }, "N", 0, TextureStorage::max_levels - 1, [&](TextureStorage::level_index_type level) {
               next_input.level = level;
            }).when(configuring_input)
              .desc("The first selected mipmap level in the next input texture will be copied to this mipmap level in the in-memory texture.")
              .extra(Cell() << "If not specified, and part of the filename matches" << fg_green << " /-(m|level)\\d+/ " << reset
                            << "then that index will be used, otherwise defaults to 0.  If multiple mipmap levels are selected from the next input texture, "
                               "they will be copied to subsequent levels."))

         (numeric_param<TextureStorage::layer_index_type> ({ }, { "first-layer" }, "N", next_input.first_layer, 0, TextureStorage::max_layers - 1).when(configuring_input)
            .desc("Skips any layer indices less than the specified value, in the next input texture."))
         (numeric_param<TextureStorage::layer_index_type> ({ }, { "last-layer" }, "N", next_input.last_layer, 0, TextureStorage::max_layers - 1).when(configuring_input)
            .desc("Skips any layer indices greater than the specified value, in the next input texture."))

         (numeric_param<TextureStorage::face_index_type> ({ }, { "first-face" }, "N", next_input.first_face, 0, TextureStorage::max_faces - 1).when(configuring_input)
            .desc("Skips any face indices less than the specified value, in the next input texture."))
         (numeric_param<TextureStorage::face_index_type> ({ }, { "last-face" }, "N", next_input.last_face, 0, TextureStorage::max_faces - 1).when(configuring_input)
            .desc("Skips any face indices greater than the specified value, in the next input texture."))

         (numeric_param<TextureStorage::level_index_type> ({ }, { "first-level" }, "N", next_input.first_level, 0, TextureStorage::max_levels - 1).when(configuring_input)
            .desc("Skips any level indices less than the specified value, in the next input texture."))
         (numeric_param<TextureStorage::level_index_type> ({ }, { "last-level" }, "N", next_input.last_level, 0, TextureStorage::max_levels - 1).when(configuring_input)
            .desc("Skips any level indices greater than the specified value, in the next input texture."))

         (enum_param<FieldType> ({ "0" }, { "field-0" }, "TYPE", nullptr, [&](FieldType ctype) {
               next_input.field_types[0] = static_cast<U8>(ctype);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat the first field as a different data type."))
         (enum_param<FieldType> ({ "1" }, { "field-1" }, "TYPE", nullptr, [&](FieldType ctype) {
               next_input.field_types[1] = static_cast<U8>(ctype);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat the second field as a different data type."))
         (enum_param<FieldType> ({ "2" }, { "field-2" }, "TYPE", nullptr, [&](FieldType ctype) {
               next_input.field_types[2] = static_cast<U8>(ctype);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat the third field as a different data type."))
         (enum_param<FieldType> ({ "3" }, { "field-3" }, "TYPE", nullptr, [&](FieldType ctype) {
               next_input.field_types[3] = static_cast<U8>(ctype);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat the fourth field as a different data type."))

         (enum_param<Swizzle> ({ "r" }, { "swizzle-r" }, "SWIZZLE", nullptr, [&](Swizzle swizzle) {
               next_input.swizzles.r = static_cast<U8>(swizzle);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to change the field corresponding to the red channel."))
         (enum_param<Swizzle> ({ "g" }, { "swizzle-g" }, "SWIZZLE", nullptr, [&](Swizzle swizzle) {
               next_input.swizzles.g = static_cast<U8>(swizzle);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to change the field corresponding to the green channel."))
         (enum_param<Swizzle> ({ "b" }, { "swizzle-b" }, "SWIZZLE", nullptr, [&](Swizzle swizzle) {
               next_input.swizzles.b = static_cast<U8>(swizzle);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to change the field corresponding to the blue channel."))
         (enum_param<Swizzle> ({ "a" }, { "swizzle-a" }, "SWIZZLE", nullptr, [&](Swizzle swizzle) {
               next_input.swizzles.a = static_cast<U8>(swizzle);
               next_input.override_components = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to change the field corresponding to the alpha channel."))

         (enum_param<Colorspace> ({ "s" }, { "colorspace" }, "NAME", nullptr, [&](Colorspace colorspace) {
               next_input.colorspace = colorspace;
               next_input.override_colorspace = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat it as if it were in the specified colorspace."))

         (flag ({ }, { "premultiplied" }, [&]() {
               next_input.premultiplied = true;
               next_input.override_premultiplied = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat it as if it had premultiplied alpha."))
         (flag ({ }, { "unpremultiplied" }, [&]() {
               next_input.premultiplied = false;
               next_input.override_premultiplied = true;
            }).when(configuring_input).desc("Reinterpret the next input texture to treat it as if it had un-premultiplied alpha."))

         (enum_param ({ "t" }, { "type" }, "FILE_EXT", default_input_format)
            .when(configuring_input)
            .desc("Specifies the file type for any input files which appear after this option.")
            .extra(Cell() << "If set to " << fg_cyan << "unknown" << reset
                          << " the file type will be detected based on the contents of the file, so specifying this option explicitly usually isn't necessary."))

         (any([&](const S& str) {
               next_input.path = str;
               next_input.file_format = default_input_format;
               input_files_.push_back(next_input);
               next_input = input_file_();
               return true;
            }).when(configuring_input))

         (flag ({ }, { "" }, configure_output)
            .when(configuring_input)
            .desc("Switches to output configuration mode")
            .extra(Cell() << "If no " << fg_yellow << "--" << reset << " flag is specified, a single beTx file will be written to the same path as the first input "
                             "file, with the extension changed to " << fg_blue << "betx" << reset << ".  If this flag is specified, but no outputs are named, "
                             "a dry-run will be performed and information about the output will be printed."))

         (doc (ids::cli_describe_section_options_compact, Cell() << fg_gray << "OUTPUT OPTIONS"))
         (doc (ids::cli_describe_section_options_manstyle, Cell() << fg_gray << "OUTPUT OPTIONS"))
         (doc (ids::cli_describe_section_options_manstyle, ""))

         (enum_param<TextureClass> ({ "x" }, { "texture-class" }, "CLASS", nullptr, [this](TextureClass tex_class) {
               tex_class_ = tex_class;
               override_tex_class_ = true;
            }).when(configuring_output).desc("Specifies the texture class for output textures."))

         (enum_param<BlockPacking> ({ "p" }, { "packing" }, "PACKING", packing_, [](BlockPacking packing) {
               return !is_compressed(packing);
            }, [this](BlockPacking packing) {
               override_block_ = true;
               return packing;
            }).when(configuring_output)
              .desc("Specifies that output textures should use a custom texel format and sets the block packing for that format."))

         (numeric_param ({ "c" }, { "components" }, "N", components_, (U8)1, (U8)4)
            .when(configuring_output).desc("Specifies the number of components when using a custom texel format."))

         (enum_param<FieldType> ({ "0" }, { "field-0" }, "TYPE", nullptr, [this](FieldType ctype) {
               field_types_[0] = static_cast<U8>(ctype);
            }).when(configuring_output).desc("Specifies the data type for the first field when using a custom texel format."))

         (enum_param<FieldType> ({ "1" }, { "field-1" }, "TYPE", nullptr, [this](FieldType ctype) {
               field_types_[1] = static_cast<U8>(ctype);
            }).when(configuring_output).desc("Specifies the data type for the second field when using a custom texel format."))
         (enum_param<FieldType> ({ "2" }, { "field-2" }, "TYPE", nullptr, [this](FieldType ctype) {
               field_types_[2] = static_cast<U8>(ctype);
            }).when(configuring_output).desc("Specifies the data type for the third field when using a custom texel format."))
         (enum_param<FieldType> ({ "3" }, { "field-3" }, "TYPE", nullptr, [this](FieldType ctype) {
               field_types_[3] = static_cast<U8>(ctype);
            }).when(configuring_output).desc("Specifies the data type for the fourth field when using a custom texel format."))

         (enum_param<Swizzle> ({ "r" }, { "swizzle-r" }, "SWIZZLE", nullptr, [this](Swizzle swizzle) {
               swizzles_.r = static_cast<U8>(swizzle);
            }).when(configuring_output).desc("Specifies the field corresponding to the red channel when using a custom texel format."))
         (enum_param<Swizzle> ({ "g" }, { "swizzle-g" }, "SWIZZLE", nullptr, [this](Swizzle swizzle) {
               swizzles_.g = static_cast<U8>(swizzle);
            }).when(configuring_output).desc("Specifies the field corresponding to the green channel when using a custom texel format."))
         (enum_param<Swizzle> ({ "b" }, { "swizzle-b" }, "SWIZZLE", nullptr, [this](Swizzle swizzle) {
               swizzles_.b = static_cast<U8>(swizzle);
            }).when(configuring_output).desc("Specifies the field corresponding to the blue channel when using a custom texel format."))
         (enum_param<Swizzle> ({ "a" }, { "swizzle-a" }, "SWIZZLE", nullptr, [this](Swizzle swizzle) {
               swizzles_.a = static_cast<U8>(swizzle);
            }).when(configuring_output).desc("Specifies the field corresponding to the alpha channel when using a custom texel format."))

         (numeric_param<U8> ({ }, { "block-span" }, "BYTES", block_span_, 0, ImageFormat::max_block_size)
            .when(configuring_output).desc("Specifies the block span when using a custom texel format.")
            .extra("If 0, defaults to the minimum size required by the block packing selected."))

         (enum_param<Colorspace> ({ "s" }, { "colorspace" }, "NAME", nullptr, [this](Colorspace colorspace) {
               colorspace_ = colorspace_;
               override_colorspace_ = true;
            }).when(configuring_output).desc("Specifies the output colorspace."))

         (flag ({ }, { "premultiplied" }, [this]() {
               premultiplied_ = true;
               override_premultiplied_ = true;
            }).when(configuring_output).desc("Output textures should be premultiplied."))
         (flag ({ }, { "unpremultiplied" }, [this]() {
               premultiplied_ = false;
               override_premultiplied_ = true;
            }).when(configuring_output).desc("Output textures should not be premultiplied."))

         (numeric_param<U8> ({ }, { "line-align" }, "BITS", line_alignment_bits_, 0, TextureAlignment::max_alignment_bits).when(configuring_output)
            .desc("Specifies the minimum alignment of each line."))
         (numeric_param<U8> ({ }, { "plane-align" }, "BITS", plane_alignment_bits_, 0, TextureAlignment::max_alignment_bits).when(configuring_output)
            .desc("Specifies the minimum alignment of each plane."))
         (numeric_param<U8> ({ }, { "level-align" }, "BITS", level_alignment_bits_, 0, TextureAlignment::max_alignment_bits).when(configuring_output)
            .desc("Specifies the minimum alignment of each level."))
         (numeric_param<U8> ({ }, { "face-align" }, "BITS", face_alignment_bits_, 0, TextureAlignment::max_alignment_bits).when(configuring_output)
            .desc("Specifies the minimum alignment of each face."))
         (numeric_param<U8> ({ }, { "layer-align" }, "BITS", layer_alignment_bits_, 0, TextureAlignment::max_alignment_bits).when(configuring_output)
            .desc("Specifies the minimum alignment of each layer."))
         (flag ({ }, { "line-align", "plane-align", "level-align", "face-align", "layer-align" }, override_alignment_).when(configuring_output))

         (numeric_param<TextureStorage::layer_index_type> ({ "l" }, { "layer" }, "N", 0, TextureStorage::max_layers - 1, [&](TextureStorage::layer_index_type layer) {
               next_output.base_layer = layer;
               next_output.layers = 1;
            }).when(configuring_output).desc("Specifies a single layer to write to the next output file.")
              .extra(Cell() << "Equivalent to " << fg_yellow << "--base-layer " << fg_cyan << "N" << fg_yellow << " --layers " << fg_cyan << "1" << reset << nl
                            << "If none of " << fg_yellow << "--layer" << reset << ", " << fg_yellow << "--base-layer" << reset << ",  or " << fg_yellow
                            << "--layers" << reset << " are specified, and the filename matches" << fg_green << " /-(l|layer)\\d+/ " << reset
                            << "then only that layer will be written to the file."))

         (numeric_param<TextureStorage::face_index_type> ({ "f" }, { "face" }, "N", 0, TextureStorage::max_faces - 1, [&](TextureStorage::face_index_type face) {
               next_output.base_face = face;
               next_output.faces = 1;
            }).when(configuring_output).desc("Specifies a single face to write to the next output file.")
              .extra(Cell() << "Equivalent to " << fg_yellow << "--base-face " << fg_cyan << "N" << fg_yellow << " --faces " << fg_cyan << "1" << reset << nl
                     << "If none of " << fg_yellow << "--face" << reset << ", " << fg_yellow << "--base-face" << reset << ",  or " << fg_yellow
                     << "--faces" << reset << " are specified, and the filename matches" << fg_green << " /-(f|face)\\d+/ " << reset
                     << "then only that face will be written to the file."))

         (numeric_param<TextureStorage::level_index_type> ({ "m" }, { "level" }, "N", 0, TextureStorage::max_levels - 1, [&](TextureStorage::level_index_type level) {
               next_output.base_level = level;
               next_output.levels = 1;
            }).when(configuring_output).desc("Specifies a single mipmap level to write to the next output file.")
              .extra(Cell() << "Equivalent to " << fg_yellow << "--base-level " << fg_cyan << "N" << fg_yellow << " --levels " << fg_cyan << "1" << reset << nl
                     << "If none of " << fg_yellow << "--level" << reset << ", " << fg_yellow << "--base-level" << reset << ",  or " << fg_yellow
                     << "--levels" << reset << " are specified, and the filename matches" << fg_green << " /-(m|level)\\d+/ " << reset
                     << "then only that level will be written to the file."))

         (numeric_param<TextureStorage::layer_index_type> ({ }, { "base-layer" }, "N", next_output.base_layer, 0, TextureStorage::max_layers - 1).when(configuring_output)
            .desc("Specifies the first layer index to write to the next output file."))
         (numeric_param<TextureStorage::layer_index_type> ({ }, { "layers" }, "N", next_output.layers, 1, TextureStorage::max_layers).when(configuring_output)
            .desc("Specifies the maximum number of layers to write to the next output file.")
            .extra("If writing multiple layers to a file format which does not support layers, multiple files will be written, with '-layer' followed by the layer index appended to the filename."))

         (numeric_param<TextureStorage::face_index_type>({ }, { "base-face" }, "N", next_output.base_face, 0, TextureStorage::max_faces - 1).when(configuring_output)
            .desc("Specifies the first face index to write to the next output file."))
         (numeric_param<TextureStorage::face_index_type> ({ }, { "faces" }, "N", next_output.faces, 1, TextureStorage::max_faces).when(configuring_output).
            desc("Specifies the maximum number of faces to write to the next output file.")
            .extra("If writing multiple faces to a file format which does not support faces, multiple files will be written, with '-face' followed by the face index appended to the filename."))

         (numeric_param<TextureStorage::level_index_type> ({ }, { "base-level" }, "N", next_output.base_level, 0, TextureStorage::max_levels - 1).when(configuring_output)
            .desc("Specifies the first mipmap level index to write to the next output file."))
         (numeric_param<TextureStorage::level_index_type> ({ }, { "levels" }, "N", next_output.levels, 1, TextureStorage::max_levels).when(configuring_output)
            .desc("Specifies the maximum number of mipmap levels to write to the next output file.")
            .extra("If writing multiple mipmap levels to a file format which does not support mipmaps, multiple files will be written, with '-level' followed by the level index appended to the filename."))

         (flag ({ "l" }, { "layer", "base-layer", "layers" }, next_output.force_layers).when(configuring_output))
         (flag ({ "f" }, { "face", "base-face", "faces" }, next_output.force_faces).when(configuring_output))
         (flag ({ "m" }, { "level", "base-level", "levels" }, next_output.force_levels).when(configuring_output))

         (flag ({ "E" }, { "big-endian" }, next_output.byte_order, bo::Big::value)
            .when(configuring_output).desc("If the next file format written supports multiple byte-orderings, use big-endian encoding instead of host-preferred encoding."))
         (flag ({ "e" }, { "little-endian" }, next_output.byte_order, bo::Little::value)
            .when(configuring_output).desc("If the next file format written supports multiple byte-orderings, use little-endian encoding instead of host-preferred encoding."))

         (flag ({ "z" }, { "compress" }, next_output.payload_compression)
            .when(configuring_output).desc("Enables optional payload compression if the next file format written supports it."))

         (enum_param<TextureFileFormat> ({ "t" }, { "type" }, "FILE_EXT", default_output_format, [](TextureFileFormat format) {
               switch (format) {
                  case TextureFileFormat::unknown:
                  case TextureFileFormat::betx:
                  case TextureFileFormat::ktx:
                  case TextureFileFormat::dds:
                  case TextureFileFormat::png:
                  case TextureFileFormat::tga:
                  case TextureFileFormat::hdr:
                  case TextureFileFormat::bmp:
                     return true;
                  default:
                     return false;
               }
            }).when(configuring_output)
              .desc("Specifies the file type for any input files which appear after this option.")
              .extra(Cell() << "If set to " << fg_cyan << "unknown" << reset
                            << " the file type will be detected based on the output file extension."))

         (any([&](const S& str) {
               next_output.path = str;
               next_output.file_format = default_output_format;
               output_files_.push_back(next_output);
               next_output = output_file_();
               return true;
            }).when(configuring_output))

         (doc (ids::cli_describe_section_options_compact, Cell() << fg_gray << "MISC OPTIONS"))
         (doc (ids::cli_describe_section_options_manstyle, Cell() << fg_gray << "MISC OPTIONS"))
         (doc (ids::cli_describe_section_options_manstyle, ""))

         (param ({ "D" },{ "input-dir" }, "PATH", [&](const S& str) {
               util::parse_multi_path(str, input_search_paths_);
            }).desc("Specifies a search path in which to search for input files.")
              .extra(Cell() << nl << "Multiple input directories may be specified by separating them with ';' or ':', or by using multiple " << fg_yellow << "--input-dir" << reset
                            << " options.  Directories will be searched in the order they are specified.  If no input directories are specified, the working directory "
                               "is implicitly searched.  Directories added to the search path apply to all inputs, including those specified earlier on the command line."))

         (param ({ "d" },{ "output-dir" }, "PATH", [&](const S& str) {
               if (!output_path_base_.empty()) {
                  throw std::runtime_error("An output directory has already been specified");
               }
               output_path_base_ = util::parse_path(str);
            }).desc("Specifies a directory to resolve relative output paths.")
              .extra(Cell() << nl << "If no output directory is specified files will be saved in the working directory.  Only one output directory may be specified, "
                                     "and it applies to all outputs, including those specified earlier on the command line."))

         (flag ({ "F" }, { "overwrite" }, overwrite_output_files_)
            .desc("Overwrites output files that already exist."))

         (verbosity_param ({ "v" },{ "verbosity" }, "LEVEL", default_log().verbosity_mask()))

         (flag ({ "V" },{ "version" }, show_version).desc("Prints version information to standard output."))

         (param ({ "?" },{ "help" }, "OPTION",
            [&](const S& value) {
               show_help = true;
               help_query = value;
            }).default_value(S())
              .allow_options_as_values(true)
              .desc(Cell() << "Outputs this help message.  For more verbose help, use " << fg_yellow << "--help")
              .extra(Cell() << nl << "If " << fg_cyan << "OPTION" << reset
                            << " is provided, the options list will be filtered to show only options that contain that string."))

         (flag ({ },{ "help" }, verbose).ignore_values(true))

         (exit_code (status_ok, "There were no errors."))
         (exit_code (status_warning, "All outputs were written, but at least one warning or notice was generated."))
         (exit_code (status_exception, "An unexpected error occurred."))
         (exit_code (status_cli_error, "There was a problem parsing the command line arguments."))
         (exit_code (status_no_output, "No output files were specified."))
         (exit_code (status_no_input, "No input files were specified, or none of the inputs could be loaded."))
         (exit_code (status_read_error, "An error occurred while reading an input file."))
         (exit_code (status_conversion_error, "An error occurred while converting or merging input textures."))
         (exit_code (status_write_error, "An error occurred while writing an output file."))

         (example (Cell() << fg_gray << "tex-level0.png tex-level1.png tex-level2.png",
            "Assembles 3 images representing consecutive mipmap levels of a texture and writes result to a file named 'tex.betx' in the working directory."))
         (example (Cell() << fg_gray << "tex.ktx" << fg_yellow << " -- " << fg_gray << "tex.png",
            "Extracts each layer, face, and level from a KTX texture and writes them to a series of PNG files named 'tex-layerL-faceF-levelM.png' in the working directory."))
         (example (Cell() << fg_gray << "tex.bmp" << fg_yellow << " -- " << fg_gray << "tex.tga",
            "Converts a DIB to Targa format."))
         ;
#pragma endregion

      proc.process(argc, argv);

      if (!show_help && !show_version && input_files_.empty()) {
         show_help = true;
         show_version = true;
         set_status_(status_no_input);
      }

      if (show_version) {
         proc
            (prologue (BE_ATEX_VERSION_STRING).query())
            (prologue (BE_GFX_VERSION_STRING).query())
            (license (BE_LICENSE).query())
            (license (BE_COPYRIGHT).query())
            ;
      }

      if (show_help) {
         proc.describe(std::cout, verbose, help_query);
      } else if (show_version) {
         proc.describe(std::cout, verbose, ids::cli_describe_section_prologue);
         proc.describe(std::cout, verbose, ids::cli_describe_section_license);
      }

      if (!input_files_.empty() && output_files_.empty() && configuring_input()) {
         next_output.file_format = TextureFileFormat::betx;
         next_output.path = input_files_.front().path;

         std::regex re("-(?:[lfm]|layer|face|level)\\d+", std::regex_constants::icase);
         S filename = std::regex_replace(next_output.path.filename().string(), re, "");
         next_output.path = next_output.path.parent_path() / filename;
         next_output.path.replace_extension("betx");

         output_files_.push_back(next_output);
      }

   } catch (const cli::OptionError& e) {
      set_status_(status_cli_error);
      log_exception(e);
   } catch (const cli::ArgumentError& e) {
      set_status_(status_cli_error);
      log_exception(e);
   } catch (const FatalTrace& e) {
      set_status_(status_cli_error);
      log_exception(e);
   } catch (const RecoverableTrace& e) {
      set_status_(status_cli_error);
      log_exception(e);
   } catch (const fs::filesystem_error& e) {
      set_status_(status_cli_error);
      log_exception(e);
   } catch (const std::system_error& e) {
      set_status_(status_cli_error);
      log_exception(e);
   } catch (const std::exception& e) {
      set_status_(status_cli_error);
      log_exception(e);
   }
}

} // be::atex
