/// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file  protocols/loops/loop_closure/loophash/LoopHashLoopClosureMover.cc
/// @brief loop closure using (tyka's) loophash library
/// @author Sachko Honda (honda@apl.washington.edu)

// project headers
#include <devel/loophash_loopclosure/LoopHashLoopClosureMover.hh>
#include <devel/loophash_loopclosure/LoopHashLoopClosureMoverCreator.hh>

// Project Headers
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/viewer/viewers.hh>
#include <core/types.hh>

#include <devel/init.hh>
#include <basic/options/option.hh>

// Utility Headers

// Unit Headers
#include <protocols/moves/Mover.fwd.hh>
#include <protocols/jobdist/standard_mains.hh>

#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/forge/build/Interval.hh>
#include <protocols/forge/remodel/RemodelMover.hh>
#include <protocols/forge/remodel/RemodelLoopMover.hh>
#include <protocols/forge/methods/util.hh>

#include <basic/options/keys/remodel.OptionKeys.gen.hh>
#include <basic/options/keys/lh.OptionKeys.gen.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>

#include <utility/vector1.hh>
#include <utility/tag/Tag.hh>
#include <basic/Tracer.hh>

#include <core/import_pose/import_pose.hh>
#include <core/conformation/Residue.hh>

// C++ headers
#include <string>
#include <fstream>

namespace devel{
namespace loophash_loopclosure{

static basic::Tracer TR ("devel.loophash_loopclosure.LoopHashLoopClosureMover" );

LoopHashLoopClosureMoverCreator::LoopHashLoopClosureMoverCreator()
{}
LoopHashLoopClosureMoverCreator::~LoopHashLoopClosureMoverCreator()
{}
protocols::moves::MoverOP
LoopHashLoopClosureMoverCreator::create_mover() const
{
	return new LoopHashLoopClosureMover();
}

std::string
LoopHashLoopClosureMoverCreator::keyname() const
{
	return LoopHashLoopClosureMoverCreator::mover_name();
}
std::string
LoopHashLoopClosureMoverCreator::mover_name() 
{
	return "LoopHashLoopClosureMover";
}

LoopHashLoopClosureMover::LoopHashLoopClosureMover()
{
}
LoopHashLoopClosureMover::~LoopHashLoopClosureMover()
{}
void
LoopHashLoopClosureMover::apply( core::pose::Pose & pose )
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	if( !option[ remodel::blueprint ].user() ) {
		TR.Error << "remodel:blueprint must be specified" << std::endl;
		utility_exit();
	}

	// Use hashloop always because this mover is all about it.
  if( !option[ remodel::RemodelLoopMover::use_loop_hash ] ) {
		TR.Error << "This mover requires remodel::RemodelLoopMover::use_loop_hash option to be true.  Flipping the flag." << std::endl;
    utility_exit();
	}
  if( !option[ lh::db_path ].user() ){
		TR.Error << "loophash_db_path (path to the loophash library) must be specified." << std::endl;
		utility_exit();
  }

	if( !option[ remodel::lh_ex_limit ] > 4 ){
		TR.Warning << "lh_ex_limit may be too big and cause segmentation error: " << option[ remodel::lh_ex_limit ] << std::endl;
	}

	remodel_ = new protocols::forge::remodel::RemodelMover();
	remodel_->apply(pose);
	//protocols::forge::remodel::RemodelLoopMoverOP loop_mover = new protocols::forge::remodel::RemodelLoopMover(loops_);
	//loop_mover->apply(pose);
}
std::string
LoopHashLoopClosureMover::get_name() const
{
	return "LoopHashLoopClosureMover";
}
protocols::moves::MoverOP
LoopHashLoopClosureMover::clone() const {
	return new LoopHashLoopClosureMover( *this );
}

protocols::moves::MoverOP
LoopHashLoopClosureMover::fresh_instance() const {
	return new LoopHashLoopClosureMover();
}
const std::vector<std::string> 
LoopHashLoopClosureMover::tokenize( const std::string& in_str, 
                               const std::string& delimiters ) const
{
  using std::endl;
  using std::map;
  using std::vector;
  using std::string;
  vector<string> tokens;

  if( delimiters == "" ) return tokens;
  if( in_str == "" ) return tokens;

  map<char, char> delim_lookup;
  for( Size i=0; i<delimiters.size(); i++ ) {
    delim_lookup[delimiters[i]] = delimiters[i];
  }

  std::string token = "";
  Size n = in_str.size();
  for( Size i=0; i<n; i++ ) {
    if( delim_lookup.find( in_str[i] ) != delim_lookup.end() ) {
      if( token != "" ) {
        tokens.push_back(token);
        token = "";
      }
    }
    else{
      token += in_str[i];
    }
  }
  if( token != "" ) {
    tokens.push_back(token);
  }
  return tokens;
}


const std::vector<MyLoop>
LoopHashLoopClosureMover::make_loops(const std::string & in_str) const {
  using namespace std;
 
  std::vector<std::string> tuples = tokenize(in_str, " ,");
  for( Size i=0; i<tuples.size(); i++ ) {
    TR.Debug << "tuple[" << i << "] = " << tuples[i] << endl;
  }
  // iterate over tuples and create a Loop object for each
  vector<MyLoop> loop_list;
  for( Size i=0; i<tuples.size(); i++ ) {
    std::vector<std::string> rcl = tokenize(tuples[i], ": " );
    runtime_assert(rcl.size() >=3);
		MyLoop loop(atoi(rcl[0].c_str()),
							  atoi(rcl[0].c_str())+1,
								rcl[1].c_str()[0],
								atoi(rcl[2].c_str()));
    loop_list.push_back( loop );
  }
  return loop_list;
}

const std::string
LoopHashLoopClosureMover::make_blueprint( const core::pose::Pose& pose, const std::string& loop_insert_instruction ) const {
	static std::string const chains( " ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890" );

	std::string bpname = "dummy.bp";
	std::ofstream bp( bpname.c_str() );
	runtime_assert( bp.good() );
    int loop_len = 0;
    size_t iChar=0;
    char cur_char = loop_insert_instruction[iChar];
    for ( size_t i=1; i<= pose.total_residue(); ++i ) {
      core::conformation::Residue const & rsd( pose.residue(i) );
      if ( chains[rsd.chain()] == cur_char ) {
			  if( ( loop_insert_instruction.length() > iChar + 1 && loop_insert_instruction[iChar + 1] < 'A' )
						&& ( i+1 < pose.total_residue() && chains[ pose.residue(i+1).chain() ] != cur_char  ) ) {
						bp << i << " A L " << std::endl;
        }
        else {
					bp << i << " A . " << std::endl;
				}
      }
      else if( loop_insert_instruction.length() < iChar + 1 ) {
        bp << i << " A . " << std::endl;
      }
      else {
        cur_char = loop_insert_instruction[++iChar];

        if( cur_char >= '1' && cur_char <= '9' ){
          std::string alphanum = "";
          while( cur_char >= '1' && cur_char <= '9' ){
            alphanum += cur_char;
            cur_char = loop_insert_instruction[++iChar];
          }
          loop_len = atoi(alphanum.c_str());
          for( size_t j=0; j<(size_t)loop_len; ++j ) {
            bp << "0 x L" << std::endl;
          }
					bp << i << " A ." << std::endl;
        }
      }
    }
	bp.close();
	return bpname;
}
const std::string
LoopHashLoopClosureMover::make_blueprint( const core::pose::Pose& pose, const std::vector<MyLoop> & loops ) const {
  static std::string const chains( " ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890" );

	// Make a fast lookup table by staring res number.
  // Validate the start and end res numbers relationship at the same time.
  // As of Mar 6, 2013, this system cannot build a loop between non-adjacent residues.
	std::map<Size, MyLoop> loop_by_r0;
	for(Size i=0; i<loops.size(); ++i) {
    if( loops[i].r1_ - loops[i].r0_ != 1 ) {
			TR.Error << "This version does not support building a loop between non-adjacent residues: ";
			TR.Error << "start=" << loops[i].r0_ << ",end=" << loops[i].r1_ << ",chain=" << loops[i].chain_ << ",length=" << loops[i].len_ << std::endl;
			TR.flush();
			utility_exit();
    }
		loop_by_r0[loops[i].r0_] = loops[i];
  }
	std::string bpname = "dummy.bp";
	std::ofstream bp( bpname.c_str() );
	runtime_assert( bp.good() );
	for ( size_t i=1; i<= pose.total_residue(); ++i ) {
		if( loop_by_r0.find(i) == loop_by_r0.end() ) {
			bp << i << " A ." << std::endl;
			continue;
		}
		MyLoop loop = loop_by_r0[i];
		if( loop.chain_ != chains[ pose.residue(i).chain() ] ) {
			TR.Error << "Residue " << loop.r0_ << " is not in the chain " << loop.chain_ << ".  Ignore the loop creating instruction." << std::endl;
			utility_exit();
		}
		bp << i << " A L" << std::endl;
    		// replace the following residues upto the loop terminal with Ls
		for(Size j=0; j<loop.len_; ++j) {
			bp << "0 x L" << std::endl;
		}
    		// jump to the terminal-1 (for the FOR loop increment)
    		i= loop.r1_-1;
	}
	bp.close();
	return bpname;
}

void LoopHashLoopClosureMover::parse_my_tag(	utility::tag::TagPtr const tag,
																protocols::moves::DataMap & ,
	    	                        protocols::filters::Filters_map const & ,
  	    	                      protocols::moves::Movers_map const &,
    	    	                    core::pose::Pose const & pose ) {

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

  // This mover has multiple modes of specifing loops which are mutually exclusive
	// * loop_insert := loop length in between chains
	// * blueprint := blueprint file
  // * loop_insert_rcn := Residue-Chain-Length
	// Currently, fixed loop length is supported.   
	if( tag->hasOption("loop_insert") + tag->hasOption("loop_insert_rcn") + tag->hasOption("blueprint") > 1 ) {
		TR.Error << "\"loop_insert\", \"loop_insert_rcn\" and \"blueprint\" options are mutually exclusive." << std::endl;
		utility_exit();
	}
  std::string bpname = "dummy.bp";
  protocols::loops::LoopsOP loops;
	if( tag->hasOption("loop_insert_rcn") ) {
		// Instruction string in Residue:Chain:Length format: 
    // e.g.  25:A:6,50:B:7 for a loop of size 6 residues after 25 (and before 26, implicit)
		// and another of size 7 residues between 50 and 51.
  	std::string loop_insert_instruction = tag->getOption<std::string>("loop_insert_rcn");
		std::vector<MyLoop> loops = make_loops(loop_insert_instruction);
		bpname = make_blueprint(pose, loops);
		TR << "Use loop_insert_rcn string (and generate " << bpname << " blueprint file)." << std::endl;
	}
  else if( tag->hasOption("loop_insert") ) {
		// Instruction string in Between Chains format: 
    // e.g. "A6B7CDE" to insert a loop of size 6 between chain A and B and another of 7 between B and C.
		std::string loop_insert_instruction = tag->getOption<std::string>("loop_insert");
		bpname = make_blueprint(pose, loop_insert_instruction);
		TR << "User loop_insert string (and generate " << bpname << " blueprint file)." << std::endl;
  }
	else if( tag->hasOption("blueprint") ) {
		bpname = tag->getOption<std::string>("blueprint");
		TR << "Use blueprint file: " << bpname << std::endl;
	}
	if( bpname == "" ) {
		TR.Error << "You must specify either \"loop_insert\" string or blueprint!" << std::endl;
		utility_exit();
	}
	option[ OptionKeys::remodel::blueprint ]( bpname );

	bool is_quick_and_dirty = tag->getOption<bool>("quick_and_dirty", true);
	option[ remodel::quick_and_dirty ]( is_quick_and_dirty );

	// Use hashloop always because this mover is all about it.
	if( option[ remodel::RemodelLoopMover::use_loop_hash ].user() && !option[ remodel::RemodelLoopMover::use_loop_hash ] ) {
		TR.Warning << "remodel::RemodelLoopMover::use_loop_hash is given false, but this mover requires true. Override user setting." << std::endl;
		TR.flush();
	}
	option[ remodel::RemodelLoopMover::use_loop_hash ]( true );

	// Use sidechains from input
	option[ packing::use_input_sc ]( true );

	Size num_trajectory = tag->getOption<Size>("num_trajectory", 1);
	option[ remodel::num_trajectory ]( num_trajectory );

	Size num_save_top = tag->getOption<Size>("save_top", 1);
	option[ remodel::save_top ](num_save_top);

	bool no_optH = tag->getOption<bool>( "no_optH", false );
	option[ packing::no_optH ]( no_optH );

	if( !tag->hasOption( "loophash_db_path" ) ){
		TR.Error << "loophash_db_path (path to the loophash library) must be specified." << std::endl;
		utility_exit();
  }
	std::string loophash_db_path = tag->getOption<std::string>( "loophash_db_path" );
	option[ lh::db_path ]( loophash_db_path );

	Size loophash_ex_limit = tag->getOption<Size>( "loophash_ex_limit", 4 );
	option[ remodel::lh_ex_limit ]( loophash_ex_limit );
}

} // loophash_loopclosure
} // devel

