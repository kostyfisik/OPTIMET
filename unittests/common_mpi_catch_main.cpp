#define CATCH_CONFIG_RUNNER

#include <catch.hpp>
#include <random>
#include <memory>
#include <mpi.h>
#include "mpi/Session.h"

std::unique_ptr<std::mt19937_64> mersenne(new std::mt19937_64(0));

int main(int argc, char ** argv) {
  Catch::Session session; // There must be exactly once instance

  int returnCode = session.applyCommandLine(argc, argv);
  if(returnCode != 0) // Indicates a command line error
    return returnCode;
  mersenne.reset(new std::mt19937_64(session.configData().rngSeed));

  optimet::mpi::init(argc, argv);

  auto const result = session.run();
  optimet::mpi::finalize();
  return result;
}