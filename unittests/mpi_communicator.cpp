#include "catch.hpp"

#include "Types.h"
#include "mpi/Collectives.h"
#include "mpi/Communicator.h"
#include "mpi/GraphCommunicator.h"
#include "mpi/Session.h"

using namespace optimet;

TEST_CASE("Creates an mpi communicator") {
  CHECK(mpi::initialized());
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  mpi::Communicator const world;

  SECTION("General stuff") {
    REQUIRE(*world == MPI_COMM_WORLD);
    REQUIRE(static_cast<t_int>(world.rank()) == rank);
    REQUIRE(static_cast<t_int>(world.size()) == size);

    mpi::Communicator shallow = world;
    CHECK(*shallow == *world);
  }

  SECTION("Split") {
    auto const split = world.split(world.is_root() ? 0 : 1);
    if(world.is_root())
      CHECK(split.size() == 1);
    else {
      CHECK(split.size() == world.size() - 1);
      CHECK(split.rank() == world.rank() - 1);
    }
  }

  SECTION("Duplicate") {
    mpi::Communicator dup = world.duplicate();
    CHECK(*dup != *world);
  }
}

TEST_CASE("Broadcasting") {
  mpi::Communicator const world;
  if(world.size() == 1)
    return;

  SECTION("From root") {
    CHECK(world.broadcast(world.rank() * 2) == 0);
    CHECK(world.broadcast(world.rank() * 2 + 1) == 1);
    CHECK(world.broadcast(static_cast<double>(world.rank() * 2) + 1.5) == 1.5);

    auto const value = world.is_root() ? world.broadcast('c') : world.broadcast<char>();
    CHECK(value == 'c');
  }

  SECTION("From other") {
    // Test written expecting root is zero;
    REQUIRE(world.root_id() == 0);
    auto const root = 1u;

    CHECK(world.broadcast(world.rank() * 2u, root) == 2u);
    CHECK(world.broadcast(world.rank() * 2u + 1u, root) == 3u);
    CHECK(world.broadcast(static_cast<double>(world.rank() * 2u) + 1.5, root) == 3.5);

    auto const value = world.is_root() ? world.broadcast('c', root) : world.rank() == 1 ?
                                         world.broadcast('d', root) :
                                         world.broadcast<char>(root);
    CHECK(value == 'd');
  }

  SECTION("Matrix") {
    Matrix<t_real> input(2, 3);
    for(Matrix<>::Index i(0); i < input.rows(); ++i)
      for(Matrix<>::Index j(0); j < input.cols(); ++j)
        input(i, j) = 2 * i + j;
    Matrix<t_real> matrix =
        world.is_root() ? input : Matrix<t_real>::Zero(input.rows(), input.cols());
    auto const result = world.broadcast(matrix);
    CHECK(result.isApprox(input, 1e-12));
  }
}

TEST_CASE("Gathering") {
  mpi::Communicator const world;

  SECTION("From root") {
    auto const result = world.gather(world.rank() * 2);
    for(std::size_t i(0); i < result.size(); ++i)
      CHECK(result[i] == i * 2);
  }
}

TEST_CASE("Non-symmetric graph communicators") {

  mpi::Communicator world;
  if(world.size() < 3)
    return;

  std::vector<std::vector<int>> const sources = {{1}, {0, 2}, {0}, {}};
  std::vector<std::vector<int>> const destinations = {{1, 2}, {0}, {1}, {}};
  std::vector<std::vector<int>> const receive_count = {{6}, {3, 9}, {3}, {}};

  auto const rank = std::min<t_uint>(world.rank(), 3);
  mpi::DistGraphCommunicator graph(world, sources[rank], destinations[rank]);
  if(graph.rank() == 0) {
    CHECK(std::get<0>(graph.nedges()) == 1);
    CHECK(std::get<1>(graph.nedges()) == 2);
    CHECK(std::get<2>(graph.nedges()) == false);
  } else if(graph.rank() == 1) {
    CHECK(std::get<0>(graph.nedges()) == 2);
    CHECK(std::get<1>(graph.nedges()) == 1);
    CHECK(std::get<2>(graph.nedges()) == false);
  } else if(graph.rank() == 2) {
    CHECK(std::get<0>(graph.nedges()) == 1);
    CHECK(std::get<1>(graph.nedges()) == 1);
    CHECK(std::get<2>(graph.nedges()) == false);
  }
}

TEST_CASE("Blocking gather of scalar on graph") {

  mpi::Communicator world;
  if(world.size() < 3)
    return;

  // std::vector<std::vector<int>> const sources = {{2, 1}, {0, 2}, {0}, {}};
  // std::vector<std::vector<int>> const destinations = {{1, 2}, {0}, {0, 1}, {}};
  std::vector<std::vector<int>> const sources = {{1, 2}, {0, 2}, {0, 1}, {}};
  std::vector<std::vector<int>> const destinations = {{1, 2}, {0, 2}, {0, 1}, {}};
  std::vector<int> const values = {2, 4, 1, 3};

  auto const rank = std::min<t_uint>(world.rank(), 3);
  mpi::DistGraphCommunicator graph(world, sources[rank], destinations[rank], false);
  auto const actual = graph.allgather(values[rank]);

  CHECK(actual.size() == sources[rank].size());
  for(decltype(actual)::size_type i(0); i < actual.size(); ++i)
    CHECK(actual[i] == values[sources[rank][i]]);
}

TEST_CASE("Non-blocking gather of Eigen vectors on graph") {

  mpi::Communicator world;
  if(world.size() < 3)
    return;

  auto const size = [](t_int rank) { return 3 * (rank + 1); };
  std::vector<std::vector<int>> const sources = {{1, 2}, {0, 2}, {0}, {}};
  std::vector<std::vector<int>> const destinations = {{1, 2}, {0}, {1, 0}, {}};
  std::vector<int> const values = {3, 5, 1, 0};

  auto const rank = std::min<t_uint>(world.rank(), 3);
  mpi::DistGraphCommunicator graph(world, sources[rank], destinations[rank], false);

  auto const receive_count = graph.allgather(size(rank));
  Vector<int> input = Vector<int>::Constant(size(rank), values[rank]);
  Vector<int> result;
  if(auto request = graph.iallgatherv(input, result, receive_count)) {
    auto const N = std::accumulate(receive_count.begin(), receive_count.end(), 0u);
    CHECK(result.size() == N);
    CHECK(static_cast<bool>(request));
  }

  for(t_uint i(0), j(0); i < receive_count.size(); j += receive_count[i++])
    CHECK((result.segment(j, receive_count[i]).array() == values[sources[rank][i]]).all());
}
