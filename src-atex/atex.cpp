#include "atex_app.hpp"

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {
   be::atex::AtexApp app(argc, argv);
   return app();
}
