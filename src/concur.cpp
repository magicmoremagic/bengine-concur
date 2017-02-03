#include "concur_app.hpp"

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {
   be::concur::ConcurApp app(argc, argv);
   return app();
}
