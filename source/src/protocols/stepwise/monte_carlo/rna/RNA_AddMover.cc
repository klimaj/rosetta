// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file RNA_DeleteMover
/// @brief Adds an RNA residue from a chain terminus.
/// @detailed
/// @author Rhiju Das

#include <protocols/stepwise/monte_carlo/rna/RNA_AddMover.hh>
#include <protocols/stepwise/monte_carlo/rna/RNA_TorsionMover.hh>
#include <core/pose/full_model_info/FullModelInfoUtil.hh>
#include <protocols/stepwise/sampling/rna/StepWiseRNA_Modeler.hh>
#include <protocols/stepwise/sampling/rna/StepWiseRNA_Util.hh>
#include <protocols/stepwise/StepWiseUtil.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/MonteCarlo.fwd.hh>
#include <core/types.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/pose/full_model_info/FullModelInfo.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/kinematics/FoldTree.hh>
#include <basic/Tracer.hh>
#include <utility/string_util.hh>
#include <numeric/random/random.hh>

#include <map>

using namespace core;
using core::Real;
using utility::make_tag_with_dashes;

//////////////////////////////////////////////////////////////////////////
// Adds one residue to a 5' or 3' chain terminus, and appropriately
//  updates the pose full_model_info object.
//
// Now updated to add whole chunks (if stored in another pose), and also
//  to add nucleotides 'by jump' (a.k.a. the skip-bulge move), and also
//  inter-chain docking.
//
// Could probably be cleaned up -- append vs. prepend are pretty similar,
//  and jump-addition moves are actually identical.
//
//////////////////////////////////////////////////////////////////////////
static numeric::random::RandomGenerator RG(2555512);  // <- Magic number, do not change it!

static basic::Tracer TR( "protocols.stepwise.monte_carlo.RNA_AddMover" ) ;

namespace protocols {
namespace stepwise {
namespace monte_carlo {
namespace rna {


  //////////////////////////////////////////////////////////////////////////
  //constructor!
	RNA_AddMover::RNA_AddMover( scoring::ScoreFunctionOP scorefxn ):
		scorefxn_( scorefxn ),
		presample_added_residue_( true ),
		presample_by_swa_( false ),
		minimize_single_res_( false ),
		start_added_residue_in_aform_( false ),
		internal_cycles_( 50 ),
		rna_torsion_mover_( new RNA_TorsionMover ),
		sample_range_small_( 5.0 ),
		sample_range_large_( 40.0 ),
		kT_( 0.5 ),
		constraint_x0_( 0.0 ),
		constraint_tol_( 0.0 )
	{}

  //////////////////////////////////////////////////////////////////////////
  //destructor
  RNA_AddMover::~RNA_AddMover()
  {}

	////////////////////////////////////////////////////////////////////
	std::string
	RNA_AddMover::get_name() const {
		return "RNA_AddMover";
	}

  //////////////////////////////////////////////////////////////////////////
  void
  RNA_AddMover::apply( core::pose::Pose &  )
	{
		std::cout << "not defined yet" << std::endl;
	}

	//////////////////////////////////////////////////////////////////////
  void
  RNA_AddMover::apply( core::pose::Pose & viewer_pose, Size const res_to_add_in_full_model_numbering, Size const res_to_build_off_in_full_model_numbering )
	{

		int offset = static_cast<int>(res_to_add_in_full_model_numbering) -	static_cast<int>(res_to_build_off_in_full_model_numbering);
		runtime_assert( offset != 0 );
		AttachmentType attachment_type = (std::abs( offset ) == 1) ?
			(offset > 0 ? BOND_TO_PREVIOUS : BOND_TO_NEXT) :
			(offset > 0 ? JUMP_TO_PREV_IN_CHAIN : JUMP_TO_NEXT_IN_CHAIN );
		if ( !check_same_chain( viewer_pose, res_to_add_in_full_model_numbering, res_to_build_off_in_full_model_numbering )) attachment_type = JUMP_INTERCHAIN;
		SWA_Move swa_move( res_to_add_in_full_model_numbering,
											 Attachment( res_to_build_off_in_full_model_numbering, attachment_type ), ADD );
		apply( viewer_pose, swa_move );
	}

	//////////////////////////////////////////////////////////////////////
  void
  RNA_AddMover::apply( core::pose::Pose & viewer_pose, SWA_Move const & swa_move )
	{
		using namespace core::pose;
		using namespace core::pose::full_model_info;

		Pose pose = viewer_pose; // hard copy -- try to avoid graphics problems when variants are added.
		runtime_assert( pose.total_residue() >= 1 );

		// will record which new dofs added.
		suite_num_ = 0;
		nucleoside_num_ = 0;

		swa_move_ = swa_move;
		res_to_add_in_full_model_numbering_       = swa_move.moving_res();
		res_to_build_off_in_full_model_numbering_ = swa_move.attached_res();
		utility::vector1< Size > const & res_list = get_res_list_from_full_model_info( pose );

		runtime_assert( res_list.has_value( res_to_build_off_in_full_model_numbering_ ) );
		runtime_assert( !res_list.has_value( res_to_add_in_full_model_numbering_ ) );
		if ( swa_move.attachment_type() == JUMP_INTERCHAIN ) { runtime_assert( !check_same_chain( pose ) );
		} else { runtime_assert( check_same_chain( pose ) ); }

		// need to encapsulate the following residue addition & domain addition functions...
		if ( res_to_add_in_full_model_numbering_ > res_to_build_off_in_full_model_numbering_ ) {
			do_append(  pose );
		} else {
			do_prepend( pose );
		}
		if ( pose.total_residue() == 2 && pose.fold_tree().num_jump() == 0 ) pose.fold_tree( kinematics::FoldTree( 2 ) ); //special 'from scratch' case.

		fix_up_residue_type_variants( pose );

		viewer_pose = pose;

		if ( start_added_residue_in_aform_ ){
			if ( suite_num_ > 0 ) {
				rna_torsion_mover_->apply_suite_torsion_Aform( viewer_pose, suite_num_ );
				if ( nucleoside_num_ > 0 ) rna_torsion_mover_->apply_nucleoside_torsion_Aform( viewer_pose, nucleoside_num_ );
			}
		} else {
			if ( suite_num_ > 0 ) {
				rna_torsion_mover_->apply_random_suite_torsion( viewer_pose, suite_num_ );
				if ( nucleoside_num_ > 0 ) rna_torsion_mover_->apply_random_nucleoside_torsion( viewer_pose, nucleoside_num_ );
			}
		}

		if ( suite_num_ > 0 ) {
			rna_torsion_mover_->sample_near_suite_torsion( viewer_pose, suite_num_, sample_range_large_);
			if ( nucleoside_num_ > 0 ) rna_torsion_mover_->sample_near_nucleoside_torsion( viewer_pose, nucleoside_num_, sample_range_large_);
		}

		clear_constraints_recursively( viewer_pose );
		if ( get_native_pose() ) {
			superimpose_recursively_and_add_constraints( viewer_pose, *get_native_pose(), constraint_x0_, constraint_tol_ );
		}

		///////////////////////////////////
		// Presampling added residue
		///////////////////////////////////
		if ( presample_added_residue_ ){
			if ( presample_by_swa_ ){
				Size swa_sample_res = nucleoside_num_;
				if ( swa_sample_res == 0) swa_sample_res = suite_num_; // nucleoside_num_ = 0 in domain addition.
				sample_by_swa( viewer_pose, swa_sample_res );
			} else {
				sample_by_monte_carlo_internal( viewer_pose );
			}
		}

	}


	////////////////////////////////////////////////////////////////////////
	void
  RNA_AddMover::do_append( core::pose::Pose & pose ){

		FullModelInfo & full_model_info = nonconst_full_model_info( pose );
		std::string const & full_sequence  = full_model_info.full_sequence();
		utility::vector1< Size > const & res_list = get_res_list_from_full_model_info( pose );
		Size const res_to_build_off = res_list.index( res_to_build_off_in_full_model_numbering_ );

		Size const offset = res_to_add_in_full_model_numbering_ - res_to_build_off_in_full_model_numbering_;
		// addition to strand ending (append)
		if ( swa_move_.attachment_type() != JUMP_INTERCHAIN ){
			runtime_assert( res_to_build_off == pose.total_residue() ||
											res_list[ res_to_build_off ] < res_list[ res_to_build_off + 1 ] - 1 );
			runtime_assert( res_list[ res_to_build_off ] < full_sequence.size() );
		}

		Size const other_pose_idx = full_model_info.get_idx_for_other_pose_with_residue( res_to_add_in_full_model_numbering_ );
		if ( other_pose_idx ){ // addition of a domain (a whole sister pose)
			append_other_pose( pose, offset, other_pose_idx );
		} else { // single residue addition -- can this just be combine with above?
			append_residue( pose, offset );
		}

	}

	////////////////////////////////////////////////////////////////////////
 	void
  RNA_AddMover::do_prepend( core::pose::Pose & pose ){

		FullModelInfo & full_model_info = nonconst_full_model_info( pose );
		runtime_assert( res_to_add_in_full_model_numbering_ < res_to_build_off_in_full_model_numbering_ );
		Size const offset = res_to_build_off_in_full_model_numbering_ - res_to_add_in_full_model_numbering_;
		Size const other_pose_idx = full_model_info.get_idx_for_other_pose_with_residue( res_to_add_in_full_model_numbering_ );

		TR << "About to add onto " << res_to_build_off_in_full_model_numbering_ << " the following residue (in full model numbering) " << res_to_add_in_full_model_numbering_ << " which may be part of other pose " << other_pose_idx << std::endl;

		if ( other_pose_idx ){ // addition of a domain (a whole sister pose)
			prepend_other_pose( pose, offset, other_pose_idx );
		} else {  // single residue addition -- can this just be combine with above?
			prepend_residue( pose, offset );
		}
	}

	////////////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::append_other_pose( pose::Pose & pose, Size const offset,
																	 Size const other_pose_idx ){
		runtime_assert( other_pose_idx > 0);
		FullModelInfo & full_model_info = nonconst_full_model_info( pose );
		Pose & other_pose = *(full_model_info.other_pose_list()[ other_pose_idx ]);
		if ( swa_move_.attachment_type() == BOND_TO_PREVIOUS ){
			runtime_assert( offset == 1 );
			merge_in_other_pose_by_bond( pose, other_pose, res_to_build_off_in_full_model_numbering_ );
		} else {
			runtime_assert( swa_move_.is_jump() );
			merge_in_other_pose_by_jump( pose, other_pose,
																	 res_to_build_off_in_full_model_numbering_, res_to_add_in_full_model_numbering_ );
		}
		nonconst_full_model_info( pose ).remove_other_pose_at_idx( other_pose_idx );
		suite_num_ = 0;
		nucleoside_num_ = get_res_list_from_full_model_info( pose ).index( res_to_add_in_full_model_numbering_ );
	}

	////////////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::prepend_other_pose( pose::Pose & pose, Size const offset,
																		Size const other_pose_idx ){
		runtime_assert( other_pose_idx > 0);
		FullModelInfo & full_model_info = nonconst_full_model_info( pose );
		Pose & other_pose = *(full_model_info.other_pose_list()[ other_pose_idx ]);
		if ( swa_move_.attachment_type() == BOND_TO_NEXT ){
			runtime_assert( offset == 1 );
			merge_in_other_pose_by_bond( pose, other_pose, res_to_build_off_in_full_model_numbering_ - 1 /*merge_res*/ );
		} else {
			runtime_assert( swa_move_.is_jump() );
			merge_in_other_pose_by_jump( pose, other_pose,
																	 res_to_add_in_full_model_numbering_, res_to_build_off_in_full_model_numbering_  );
		}
		nonconst_full_model_info( pose ).remove_other_pose_at_idx( other_pose_idx );
		suite_num_ = 0;
		nucleoside_num_ = get_res_list_from_full_model_info( pose ).index( res_to_add_in_full_model_numbering_ );
	}

	////////////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::append_residue( pose::Pose & pose, Size const offset ){

		using namespace core::chemical;
		using namespace core::chemical::rna;
		using namespace protocols::stepwise::sampling::rna;

		FullModelInfo & full_model_info = nonconst_full_model_info( pose );
		std::string const & full_sequence  = full_model_info.full_sequence();
		utility::vector1< Size > const & res_list = get_res_list_from_full_model_info( pose );
		Size const res_to_build_off = res_list.index( res_to_build_off_in_full_model_numbering_ );
		Size res_to_add = res_to_build_off + 1;

		char newrestype = full_sequence[ res_to_add_in_full_model_numbering_ - 1 ];
		choose_random_if_unspecified_nucleotide( newrestype );
		chemical::AA my_aa = chemical::aa_from_oneletter_code( newrestype );
		ResidueTypeSet const & rsd_set = pose.residue_type( 1 ).residue_type_set();
		chemical::ResidueTypeCOPs const & rsd_type_list( rsd_set.aa_map( my_aa ) );

		// iterate over rsd_types, pick one.
		chemical::ResidueType const & rsd_type = *rsd_type_list[1];
		core::conformation::ResidueOP new_rsd = conformation::ResidueFactory::create_residue( rsd_type );
		Size actual_offset = offset;
		if ( swa_move_.attachment_type() == BOND_TO_PREVIOUS ){
			runtime_assert( offset == 1 );
			remove_variant_type_from_pose_residue( pose, "UPPER_TERMINUS", res_to_build_off ); // got to be safe.
			remove_variant_type_from_pose_residue( pose, "THREE_PRIME_PHOSPHATE", res_to_build_off ); // got to be safe.
			pose.append_polymer_residue_after_seqpos( *new_rsd, res_to_build_off, true /*build ideal geometry*/ );
			suite_num_ = res_to_add - 1;
		} else {
			runtime_assert( swa_move_.is_jump() );

			// following is getting really clunky. May want to instead use merge_two_poses code.
			Size takeoff_res( 0 ), k( 0 );
			for ( k = res_to_add_in_full_model_numbering_; k >= res_to_build_off_in_full_model_numbering_; k-- ){
				if ( res_list.has_value( k ) ) { takeoff_res = res_list.index( k ); break; }
			}
			actual_offset = res_to_add_in_full_model_numbering_ - k;
			res_to_add = takeoff_res + 1;
			pose.insert_residue_by_jump( *new_rsd, res_to_add, takeoff_res,
																	 default_jump_atom( pose.residue( takeoff_res ) ),
																	 default_jump_atom( *new_rsd ) );
			kinematics::FoldTree f = pose.fold_tree();
			Size const jump_nr = f.jump_nr( takeoff_res, res_to_add );
			f.slide_jump( jump_nr, res_to_build_off, res_to_add );
			f.set_jump_atoms( jump_nr, default_jump_atom( pose.residue( res_to_build_off ) ),
												default_jump_atom( pose.residue( res_to_add ) ) );
			pose.fold_tree( f );

			add_variant_type_to_pose_residue( pose, "VIRTUAL_PHOSPHATE", res_to_add );
			if ( res_to_add_in_full_model_numbering_ < res_list[ res_to_build_off+1 ]-1 ) {
				add_variant_type_to_pose_residue( pose, "VIRTUAL_RIBOSE", res_to_add );
			}
			suite_num_ = 0;
		}

		reorder_full_model_info_after_append( pose, res_to_add, actual_offset );

		nucleoside_num_ = res_to_add;
	}


	////////////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::prepend_residue( pose::Pose & pose, Size const offset ){

		using namespace core::chemical;
		using namespace core::chemical::rna;
		using namespace protocols::stepwise::sampling::rna;

		FullModelInfo & full_model_info = nonconst_full_model_info( pose );
		std::string const & full_sequence  = full_model_info.full_sequence();
		utility::vector1< Size > const & res_list = get_res_list_from_full_model_info( pose );
		Size const res_to_build_off = res_list.index( res_to_build_off_in_full_model_numbering_ );
		Size res_to_add = res_to_build_off;

		runtime_assert( res_list[ res_to_add ] > 1 );

		char newrestype = full_sequence[ res_to_add_in_full_model_numbering_ - 1 ];
		choose_random_if_unspecified_nucleotide( newrestype );

		TR.Debug << "I want to add: " << newrestype << " before " << res_to_build_off << std::endl;

		chemical::AA my_aa = chemical::aa_from_oneletter_code( newrestype );

		ResidueTypeSet const & rsd_set = pose.residue_type( 1 ).residue_type_set();
		chemical::ResidueTypeCOPs const & rsd_type_list( rsd_set.aa_map( my_aa ) );

		// iterate over rsd_types, pick one.
		chemical::ResidueType const & rsd_type = *rsd_type_list[1];
		core::conformation::ResidueOP new_rsd = conformation::ResidueFactory::create_residue( rsd_type );
		Size actual_offset = offset;
		if ( swa_move_.attachment_type() == BOND_TO_NEXT ){
			runtime_assert( offset == 1 );
			remove_variant_type_from_pose_residue( pose, "VIRTUAL_PHOSPHATE", res_to_add ); // got to be safe.
			remove_variant_type_from_pose_residue( pose, "FIVE_PRIME_PHOSPHATE", res_to_build_off ); // got to be safe.
			remove_variant_type_from_pose_residue( pose, "LOWER_TERMINUS", res_to_add ); // got to be safe.
			pose.prepend_polymer_residue_before_seqpos( *new_rsd, res_to_add, true /*build ideal geometry*/ );
			suite_num_ = res_to_add;
		} else {
			runtime_assert( swa_move_.is_jump() );

			// following is getting really clunky. May want to instead use merge_two_poses code.
			Size takeoff_res( 0 ), k( 0 );
			for ( k = res_to_add_in_full_model_numbering_; k <= res_to_build_off_in_full_model_numbering_; k++ ){
				if ( res_list.has_value( k ) ) { takeoff_res = res_list.index( k ); break; }
			}
			actual_offset = k - res_to_add_in_full_model_numbering_;
			res_to_add = takeoff_res;
			pose.insert_residue_by_jump( *new_rsd, res_to_add, takeoff_res,
																	 default_jump_atom( pose.residue( takeoff_res ) ),
																	 default_jump_atom( *new_rsd ) );
			kinematics::FoldTree f = pose.fold_tree();
			Size const jump_nr = f.jump_nr( res_to_add, takeoff_res+1 );
			f.slide_jump( jump_nr, res_to_add, res_to_build_off+1 );
			f.set_jump_atoms( jump_nr, default_jump_atom( pose.residue( res_to_add ) ),
												default_jump_atom( pose.residue( res_to_build_off+1 ) ) );
			pose.fold_tree( f );

			if ( res_to_add_in_full_model_numbering_ > res_list[ res_to_build_off-1 ]+1 ) {
				add_variant_type_to_pose_residue( pose, "VIRTUAL_RIBOSE", res_to_add );
				add_variant_type_to_pose_residue( pose, "VIRTUAL_PHOSPHATE", res_to_add+1 );
			}
			suite_num_ = 0;
		}
		reorder_full_model_info_after_prepend( pose, res_to_add, actual_offset );

		// initialize with a random torsion... ( how about an A-form + perturbation ... or go to a 'reasonable' rotamer)
		nucleoside_num_ = res_to_add;
	}

	////////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::sample_by_swa( pose::Pose & pose, Size const res_to_add ) const{
		using namespace core::pose::full_model_info;
		runtime_assert( stepwise_rna_modeler_ != 0 );
		TR.Debug << "presampling by stepwise." << res_to_add << std::endl;
		stepwise_rna_modeler_->set_moving_res_and_reset( res_to_add );
		if ( !minimize_single_res_ ) stepwise_rna_modeler_->set_minimize_res( get_moving_res_from_full_model_info( pose ) );
		stepwise_rna_modeler_->apply( pose );
	}


	////////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::sample_by_monte_carlo_internal( pose::Pose & pose ) const {

		using namespace protocols::moves;

		TR.Debug << "presampling added residue! " << nucleoside_num_ << " over " << internal_cycles_ << " cycles " << std::endl;
		MonteCarloOP monte_carlo_internal = new MonteCarlo( pose, *scorefxn_, kT_ );

		std::string move_type( "" );
		for ( Size count_internal = 1; count_internal <= internal_cycles_; count_internal++ ){

			rna_torsion_mover_->sample_near_suite_torsion( pose, suite_num_, sample_range_large_);
			if ( nucleoside_num_ > 0 ) rna_torsion_mover_->sample_near_nucleoside_torsion( pose, nucleoside_num_, sample_range_large_);
			monte_carlo_internal->boltzmann( pose, move_type );

			rna_torsion_mover_->sample_near_suite_torsion( pose, suite_num_, sample_range_small_);
			if ( nucleoside_num_ > 0 ) rna_torsion_mover_->sample_near_nucleoside_torsion( pose, nucleoside_num_, sample_range_small_);
			monte_carlo_internal->boltzmann( pose, move_type );
			//std::cout << "During presampling: " << (*scorefxn_)( pose );
		} // monte carlo cycles
	}

	///////////////////////////////////////////////////////////////////
	void
	RNA_AddMover::set_stepwise_rna_modeler( protocols::stepwise::sampling::rna::StepWiseRNA_ModelerOP stepwise_rna_modeler ){
		stepwise_rna_modeler_ = stepwise_rna_modeler;
	}

	///////////////////////////////////////////////////////////////////
	bool
	RNA_AddMover::check_same_chain( pose::Pose const & pose, Size const res_to_add_in_full_model_numbering, Size const res_to_build_off_in_full_model_numbering ){
		return 	( get_chain_for_full_model_resnum( res_to_add_in_full_model_numbering, pose ) ==
							get_chain_for_full_model_resnum( res_to_build_off_in_full_model_numbering, pose ) );
	}

	bool
	RNA_AddMover::check_same_chain( pose::Pose const & pose ) {
		return check_same_chain( pose, res_to_add_in_full_model_numbering_, res_to_build_off_in_full_model_numbering_ );
	}

} //rna
} //monte_carlo
} //stepwise
} //protocols
