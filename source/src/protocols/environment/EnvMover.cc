// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/protocols/environment/EnvMover.cc
/// @author Justin Porter

// Unit Headers
#include <protocols/environment/EnvMover.hh>
#include <protocols/environment/EnvMoverCreator.hh>

// Package headers
#include <protocols/environment/Environment.hh>
#include <protocols/environment/ClaimingMover.hh>

// Project headers
#include <core/pose/Pose.hh>

#include <utility/tag/Tag.hh>

// tracer
#include <basic/Tracer.hh>

// C++ Headers

// ObjexxFCL Headers

static thread_local basic::Tracer tr( "protocols.environment.EnvMover", basic::t_info );

namespace protocols {
namespace environment {

// creator
std::string
EnvMoverCreator::keyname() const {
  return EnvMoverCreator::mover_name();
}

protocols::moves::MoverOP
EnvMoverCreator::create_mover() const {
  return new EnvMover();
}

std::string
EnvMoverCreator::mover_name() {
  return "Environment";
}

EnvMover::EnvMover():
  Mover()
{}

EnvMover::EnvMover( EnvironmentOP env ):
  env_( env )
{}

EnvMover::~EnvMover() {}

void EnvMover::apply( Pose& pose ) {
  if( movers_.size() == 0 ){
    tr.Warning << "[WARNING] The environment " << env_->name()
               << " is being run without any registrant movers." << std::endl;
  }

  core::pose::Pose ppose = env_->start( pose );
  movers_.apply( ppose );
  pose = env_->end( ppose );
}

void EnvMover::parse_my_tag( utility::tag::TagCOP tag,
                             basic::datacache::DataMap & data,
                             Filters_map const & filters,
                             moves::Movers_map const & movers,
                             core::pose::Pose const& pose ) {
  typedef utility::vector0< TagCOP > TagCOPs;

  env_ = new Environment( tag->getOption<std::string>( "name" ) );

  env_->auto_cut( tag->getOption< bool >( "auto_cut", env_->auto_cut() ) );
  env_->inherit_cuts( tag->getOption< bool >( "inherit_cuts", env_->inherit_cuts() ) );

  TagCOPs const& subtags = tag->getTags();
  for( TagCOPs::const_iterator it = subtags.begin(); it != subtags.end(); ++it ){
    parse_subtag( *it, data, filters, movers, pose );
  }
}

moves::MoverOP find_mover( utility::tag::TagCOP tag,
                    moves::Movers_map movers ){

  std::string mover_name("[NOT_SET]");
  if ( tag->hasOption( "name" ) ){
    mover_name = tag->getOption< std::string >( "name" );
  } else if ( tag->hasOption( "mover" ) ) {
    mover_name = tag->getOption< std::string >( "mover" );
  } else {
    std::string err = "An environment was provided a subtag without the appropriate options. Either 'name' or 'mover' must be specified.";
    throw utility::excn::EXCN_RosettaScriptsOption( err );
  }
  moves::Movers_map::const_iterator mv_it = movers.find( mover_name );

  if( mv_it != movers.end() ){
    assert( mv_it->second );
    return mv_it->second;
  } else {
    std::string err = "The mover " + mover_name + " could not be found. Check your spelling in the xml script.";
    throw utility::excn::EXCN_RosettaScriptsOption( err );
  }

}

void EnvMover::parse_subtag( utility::tag::TagCOP tag,
                             basic::datacache::DataMap &,
                             filters::Filters_map const &,
                             moves::Movers_map const & movers,
                             core::pose::Pose const& ) {
  std::set< moves::MoverOP > reg_only_movers;

  try {
    if( tag->getName() == "Apply" ){
      moves::MoverOP mover = find_mover( tag, movers );
      assert( mover );
      env_->register_mover( mover );
      movers_.add_mover( mover );
      if( reg_only_movers.find( mover ) != reg_only_movers.end() ){
        tr.Warning << "[TIP] You don't need to register the mover "
                   << tag->getOption< std::string >( "name " )
                   << " with the 'Apply' tag ahead of time. The 'Apply' Environment tag does that automatically." << std::endl;
      }
    } else if (tag->getName() == "Register" ){

      moves::MoverOP mover = find_mover( tag, movers );
      env_->register_mover( mover );
      reg_only_movers.insert( mover );
    } else {
      std::ostringstream err;
      err << "The Environment cannot be used with the tag '" << *tag << "'.";
      throw utility::excn::EXCN_RosettaScriptsOption( err.str() );
    }
  } catch ( utility::excn::EXCN_Msg_Exception e ) {
    throw utility::excn::EXCN_RosettaScriptsOption( "In Environment '" + get_name() + "': " + e.msg() );
  }
}

std::string EnvMover::get_name() const {
  return "EnvMover("+env_->name()+")";
}

moves::MoverOP EnvMover::clone() const {
  return new EnvMover( *this );
}

} // environment
} // protocols
