#pragma once
#ifndef BE_CONCUR_CONCUR_APP_HPP_
#define BE_CONCUR_CONCUR_APP_HPP_

#include <be/core/lifecycle.hpp>
#include <be/core/filesystem.hpp>
#include <map>
#include <glm/vec2.hpp>

namespace be {
namespace concur {

///////////////////////////////////////////////////////////////////////////////
class ConcurApp final {
public:
   ConcurApp(int argc, char** argv);

   int operator()();

private:

   enum class input_type {
      automatic,
      bitmap,
      png
   };

   enum class output_type {
      automatic,
      icon,
      cursor
   };

   CoreInitLifecycle init_;
   I8 status_ = 0;

   std::map<Path, input_type> inputs_;
   Path output_path_;
   output_type output_type_ = output_type::automatic;
   std::map<U16, glm::vec2> output_sizes_;

};

} // be::concur
} // be

#endif
