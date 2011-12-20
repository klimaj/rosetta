// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// (C) 199x-2009 Rosetta Commons participating institutions and developers.
// For more information, see http://www.rosettacommons.org/.

/// @file   SetupNCSMover.cc
/// @brief  Sets up NCS restraints
/// @author Frank DiMaio

// Unit headers
#include <protocols/simple_moves/symmetry/SetupNCSMoverCreator.hh>
#include <protocols/simple_moves/symmetry/SetupNCSMover.hh>

// AUTO-REMOVED #include <protocols/moves/DataMap.hh>
#include <protocols/rosetta_scripts/util.hh>

#include <core/id/TorsionID.hh>
#include <core/id/AtomID.hh>
#include <core/pose/Pose.hh>
// AUTO-REMOVED #include <core/pose/PDBInfo.hh>
// AUTO-REMOVED #include <core/pose/symmetry/util.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/Conformation.hh>
// AUTO-REMOVED #include <core/conformation/symmetry/util.hh>

#include <core/scoring/constraints/DihedralPairConstraint.hh>
#include <core/scoring/constraints/TopOutFunc.hh>

#include <utility/vector1.hh>
#include <utility/exit.hh>
#include <utility/tag/Tag.hh>

#include <basic/Tracer.hh>

#include <utility/vector0.hh>


namespace protocols {
namespace simple_moves {
namespace symmetry {

static basic::Tracer TZ("protocols.simple_moves.symmetry.SetupNCSMover");

// creators
std::string
SetupNCSMoverCreator::keyname() const {
	return SetupNCSMoverCreator::mover_name();
}

protocols::moves::MoverOP
SetupNCSMoverCreator::create_mover() const {
	return new SetupNCSMover;
}

std::string
SetupNCSMoverCreator::mover_name() {
	return "SetupNCS";
}

////////////////////
////////////////////

SetupNCSMover::SetupNCSMover() : protocols::moves::Mover("SetupNCSMover") { 
	set_defaults();
}

SetupNCSMover::SetupNCSMover( std::string src, std::string tgt ) : protocols::moves::Mover("SetupNCSMover") { 
	add_group( src, tgt );
	set_defaults();
}

SetupNCSMover::SetupNCSMover( std::string src, utility::vector1<std::string> tgt ): protocols::moves::Mover("SetupNCSMover") {
	for (int i=1; i<=tgt.size(); ++i)
		add_group( src, tgt[i] );
	set_defaults();
}

SetupNCSMover::SetupNCSMover( utility::vector1<std::string> src, utility::vector1<std::string> tgt ) : protocols::moves::Mover("SetupNCSMover") {
	runtime_assert( src.size() == tgt.size() );
	for (int i=1; i<=tgt.size(); ++i)
		add_group( src[i], tgt[i] );
	set_defaults();
}

SetupNCSMover::~SetupNCSMover(){}

void SetupNCSMover::set_defaults() {
	wt_ = 0.01;  //fpd this seems reasonable in fullatom
	limit_ = 10.0;  // in degrees
	bb_ = chi_ = true;
	symmetric_sequence_ = false;
}

void SetupNCSMover::add_group( std::string src, std::string tgt ) {
	src_.push_back( src );
	tgt_.push_back( tgt );
}

void SetupNCSMover::add_groupE( std::string src, std::string tgt ) {
  srcE_.push_back( src );
  tgtE_.push_back( tgt );
}


//
void SetupNCSMover::apply( core::pose::Pose & pose ) {
	using namespace std;
	using namespace utility;
	using namespace core;
	using namespace core::id;
	using namespace core::scoring::constraints;

	assert( src_.size() == tgt_.size() );

	// map residue ranges -> resid pairs (using PDBinfo if necessary)
	for (int i=1; i<=src_.size(); ++i) {
		utility::vector1<Size> src_i = protocols::rosetta_scripts::get_resnum_list_ordered( src_[i], pose );
		utility::vector1<Size> tgt_i = protocols::rosetta_scripts::get_resnum_list_ordered( tgt_[i], pose );
		runtime_assert( src_i.size() == tgt_i.size() );

		//
		if ( src_i.size() == 0 ) {
			utility_exit_with_message("Error creating NCS constraints: " + src_[i] +" : "+tgt_[i]);
		}

		for (int j=1; j<=src_i.size(); ++j) {
			core::Size resnum_src = src_i[j], resnum_tgt = tgt_i[j];
			id::AtomID id_a1,id_a2,id_a3,id_a4, id_b1,id_b2,id_b3,id_b4;

			TZ.Debug << "Add constraint " << resnum_src << " <--> " << resnum_tgt << std::endl;


			//replace residue identity to match reference positions
			if (symmetric_sequence_) {
				pose.replace_residue( resnum_tgt, pose.residue(resnum_src), true );
			}

			if (bb_) {
				// fpd better safe than sorry
				runtime_assert (pose.residue(resnum_src).mainchain_torsions().size() == pose.residue(resnum_tgt).mainchain_torsions().size());
				for ( Size k=1, k_end = pose.residue(resnum_src).mainchain_torsions().size(); k<= k_end; ++k ) {
					id::TorsionID tors_src(resnum_src,BB,k);
					id::TorsionID tors_tgt(resnum_tgt,BB,k);

					pose.conformation().get_torsion_angle_atom_ids(tors_src,  id_a1, id_a2, id_a3, id_a4 );
					pose.conformation().get_torsion_angle_atom_ids(tors_tgt,  id_b1, id_b2, id_b3, id_b4 );

					// make the cst
					pose.add_constraint( new DihedralPairConstraint( id_a1, id_a2, id_a3, id_a4, id_b1, id_b2, id_b3, id_b4,
					                                                new TopOutFunc( wt_, 0.0, limit_ ) ) );
				}
			}

			if (chi_) {
				if ( pose.residue(resnum_src).aa() != pose.residue(resnum_tgt).aa() ) {
					TZ.Error << "Trying to constrain sidechain torsions of different residue types!" << std::endl;
					TZ.Error << " >>> " << resnum_src << " vs " << resnum_tgt << std::endl;
					continue;
				}

				// cys/cyd messes this up I think
				if ( pose.residue(resnum_src).nchi() != pose.residue(resnum_tgt).nchi() ) {
					TZ.Error << "Trying to constrain sidechains with different number of chi angles!" << std::endl;
					continue;
				}

				for ( Size k=1, k_end = pose.residue(resnum_src).nchi(); k<= k_end; ++k ) {
					id::TorsionID tors_src(resnum_src,CHI,k);
					id::TorsionID tors_tgt(resnum_tgt,CHI,k);

					pose.conformation().get_torsion_angle_atom_ids(tors_src,  id_a1, id_a2, id_a3, id_a4 );
					pose.conformation().get_torsion_angle_atom_ids(tors_tgt,  id_b1, id_b2, id_b3, id_b4 );

					// make the cst
					pose.add_constraint( new DihedralPairConstraint( id_a1, id_a2, id_a3, id_a4, id_b1, id_b2, id_b3, id_b4,
					                                                new TopOutFunc( wt_, 0.0, limit_ ) ) );
				}
			}
		}
	}


//symmetrize ONLY sequence (eg. N- and C-term)
	if (symmetric_sequence_) {

	  assert( srcE_.size() == tgtE_.size() );
	
	  // map residue ranges -> resid pairs (using PDBinfo if necessary)
	  for (int i=1; i<=srcE_.size(); ++i) {
	    utility::vector1<Size> src_i = protocols::rosetta_scripts::get_resnum_list_ordered( srcE_[i], pose );
	    utility::vector1<Size> tgt_i = protocols::rosetta_scripts::get_resnum_list_ordered( tgtE_[i], pose );
	    runtime_assert( src_i.size() == tgt_i.size() );
	
	    //
	    if ( src_i.size() == 0 ) {
	      utility_exit_with_message("Error creating NCS constraints: " + src_[i] +" : "+tgt_[i]);
	    }
	
	    for (int j=1; j<=src_i.size(); ++j) {
	      core::Size resnum_src = src_i[j], resnum_tgt = tgt_i[j];
	
	      TZ.Debug << "Symmetrizing residues " << resnum_src << " <--> " << resnum_tgt << std::endl;
	
	      //replace residue identity to match reference positions
	      pose.replace_residue( resnum_tgt, pose.residue(resnum_src), true );
			}
		}
	}


}

void SetupNCSMover::parse_my_tag( 
			utility::tag::TagPtr const tag,
			moves::DataMap & /*data*/,
			filters::Filters_map const & /*filters*/,
			moves::Movers_map const & /*movers*/,
			core::pose::Pose const & /*pose*/ ) {
	if (tag->hasOption( "bb" )) bb_ = tag->getOption< bool >( "bb" );
	if (tag->hasOption( "chi" )) chi_ = tag->getOption< bool >( "chi" );
	if (tag->hasOption( "wt" )) wt_ = tag->getOption< core::Real >( "wt" );
	if (tag->hasOption( "limit" )) limit_ = tag->getOption< core::Real >( "limit" );
	if (tag->hasOption( "symmetric_sequence" )) symmetric_sequence_ = tag->getOption< bool >( "symmetric_sequence" );


	// now parse ncs groups <<< subtags
	utility::vector1< TagPtr > const branch_tags( tag->getTags() );
	utility::vector1< TagPtr >::const_iterator tag_it;
	for( tag_it = branch_tags.begin(); tag_it!=branch_tags.end(); ++tag_it ){
		if( (*tag_it)->getName() == "NCSgroup" || (*tag_it)->getName() == "ncsgroup" ){
			std::string src_i = (*tag_it)->getOption<std::string>( "source" );
			std::string tgt_i = (*tag_it)->getOption<std::string>( "target" );
			if (symmetric_sequence_) {
				TZ.Debug << "Symmetrizing sequences " << src_i << " -> " << tgt_i << std::endl;
			}
			TZ.Debug << "Adding NCS cst " << src_i << " -> " << tgt_i << std::endl;
			add_group( src_i, tgt_i );
		}
		
		if( (*tag_it)->getName() == "NCSend" || (*tag_it)->getName() == "ncsend" ){
      std::string srcE_i = (*tag_it)->getOption<std::string>( "source" );
      std::string tgtE_i = (*tag_it)->getOption<std::string>( "target" );
      if (symmetric_sequence_) {
				TZ.Debug << "Symmetrizing sequences at ends" << srcE_i << " -> " << tgtE_i << std::endl;
			}
      add_groupE( srcE_i, tgtE_i );
		}
		
	}
}

std::string
SetupNCSMover::get_name() const {
	return SetupNCSMoverCreator::mover_name();
}


} // symmetry
} // moves
} // protocols
