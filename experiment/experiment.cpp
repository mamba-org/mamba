#include <iostream>

#include "match_spec.hpp"
#include "../src/util.cpp"
#include "output.hpp"
#include "history.hpp"


int main(int argc, char** argv) {

  mamba::MessageLogger::global_log_severity() = mamba::LogSeverity::INFO;

   mamba::History h("/home/mariana/.conda/envs/mamba/");
   std::vector<mamba::History::UserRequest> user_request;
   user_request = h.get_user_requests();
   h.add_entry(user_request);

    // mamba::MatchSpec obj_matchspec("conda-forge::foo[build=py2*]");

    // obj_matchspec.MatchSpec::parse();

  return 0;
}