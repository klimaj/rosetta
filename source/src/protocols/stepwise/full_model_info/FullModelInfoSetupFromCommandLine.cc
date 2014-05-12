// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/full_model_info/FullModelInfoSetupFromCommandLine.cc
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#include <protocols/stepwise/full_model_info/FullModelInfoSetupFromCommandLine.hh>
#include <core/pose/full_model_info/util.hh>
#include <core/pose/full_model_info/FullModelInfo.hh>
#include <core/pose/full_model_info/FullModelParameters.hh>
#include <core/pose/Pose.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <core/pose/util.hh>
#include <core/pose/rna/util.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/rna/util.hh>
#include <core/import_pose/import_pose.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/sequence/Sequence.hh>
#include <core/sequence/util.hh>
#include <utility/stream_util.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/full_model.OptionKeys.gen.hh>
#include <basic/options/keys/stepwise.OptionKeys.gen.hh>
#include <basic/datacache/BasicDataCache.hh>
#include <basic/Tracer.hh>

static basic::Tracer TR( "core.pose.full_model_info.FullModelInfoSetupFromCommandLine" );

using namespace core;
using utility::vector1;

namespace core {
namespace pose {
namespace full_model_info {


	//////////////////////////////////////////////////////////////////////////////////////
	// might be better to move these into core (e.g., core::pose::full_model_info ),
	// or into a new protocols/full_model_setup/ directory.
	core::pose::PoseOP
	get_pdb_and_cleanup( std::string const input_file,
											 core::chemical::ResidueTypeSetCAP rsd_set )
	{
		using namespace core::pose;
		PoseOP input_pose = new Pose;
		import_pose::pose_from_pdb( *input_pose, *rsd_set, input_file );
		cleanup( *input_pose );
		make_sure_full_model_info_is_setup( *input_pose );
		return input_pose;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// currently have stuff we need for RNA... put any protein cleanup here too.
	void
	cleanup( pose::Pose & pose ){
		rna::figure_out_reasonable_rna_fold_tree( pose );
		rna::virtualize_5prime_phosphates( pose );
		pose.conformation().detect_disulfides();
	}

	///////////////////////////////////////////////////////////////////////////////////////
	pose::PoseOP
	initialize_pose_and_other_poses_from_command_line( core::chemical::ResidueTypeSetCAP rsd_set ){

		using namespace basic::options;
		using namespace basic::options::OptionKeys;
		using namespace core::io::silent;

		utility::vector1< std::string > const & input_pdb_files    = option[ in::file::s ]();
		utility::vector1< std::string > const & input_silent_files = option[ in::file::silent ]();

		utility::vector1< pose::PoseOP > input_poses;
		for ( Size n = 1; n <= input_pdb_files.size(); n++ ) {
			input_poses.push_back( get_pdb_and_cleanup( input_pdb_files[ n ], rsd_set ) );
		}
		for ( Size n = 1; n <= input_silent_files.size(); n++ ) {
			PoseOP pose = new Pose;
			SilentFileData silent_file_data;
			silent_file_data.read_file( input_silent_files[n] );
			silent_file_data.begin()->fill_pose( *pose, *rsd_set );
			input_poses.push_back( pose );
		}

		if ( input_poses.size() == 0 ) input_poses.push_back( new Pose ); // just a blank pose for now.

		if ( option[ full_model::other_poses ].user() ) {
			get_other_poses( input_poses, option[ full_model::other_poses ](), rsd_set );
		}

		fill_full_model_info_from_command_line( input_poses ); 	//FullModelInfo (minimal object needed for add/delete)
		return input_poses[1];
	}

	///////////////////////////////////////////////////////////////////////////////////////
	void
	get_other_poses( utility::vector1< pose::PoseOP > & other_poses,
									 utility::vector1< std::string > const & other_files,
									core::chemical::ResidueTypeSetCAP rsd_set ){

		for ( Size n = 1; n <= other_files.size(); n++ ){
			other_poses.push_back( get_pdb_and_cleanup( other_files[ n ], rsd_set ) );
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////
	void
	fill_full_model_info_from_command_line( pose::Pose & pose ){
		vector1< PoseOP > other_pose_ops; // dummy
		fill_full_model_info_from_command_line( pose, other_pose_ops );
	}

	///////////////////////////////////////////////////////////////////////////////////////
	void
	fill_full_model_info_from_command_line( vector1< PoseOP > & pose_ops ){

		runtime_assert( pose_ops.size() > 0 );

		vector1< PoseOP > other_pose_ops;
		for ( Size n = 2; n <= pose_ops.size(); n++ ) other_pose_ops.push_back( pose_ops[n] );

		fill_full_model_info_from_command_line( *(pose_ops[1]), other_pose_ops );
	}


	///////////////////////////////////////////////////////////////////////////////////////
	// this is needed so that first pose holds pointers to other poses.
	void
	fill_full_model_info_from_command_line( pose::Pose & pose, vector1< PoseOP > & other_pose_ops ){

		vector1< Pose * > pose_pointers;
		pose_pointers.push_back( & pose );
		for ( Size n = 1; n <= other_pose_ops.size(); n++ ) pose_pointers.push_back( & (*other_pose_ops[ n ]) );

		fill_full_model_info_from_command_line( pose_pointers );

		nonconst_full_model_info( pose ).set_other_pose_list( other_pose_ops );

	}


	///////////////////////////////////////////////////////////////////////////////////////
	void
	fill_full_model_info_from_command_line( vector1< Pose * > & pose_pointers ){

		using namespace basic::options;
		using namespace basic::options::OptionKeys;

		if ( !option[ in::file::fasta ].user() ){
			for ( Size n = 1; n <= pose_pointers.size(); n++ ) make_sure_full_model_info_is_setup( *pose_pointers[n] );
			return;
		}

		std::string const fasta_file = option[ in::file::fasta ]()[1];
		core::sequence::SequenceOP fasta_sequence = core::sequence::read_fasta_file( fasta_file )[1];
		std::string const desired_sequence = fasta_sequence->sequence();

		FullModelParametersOP full_model_parameters =	new FullModelParameters( desired_sequence );

		vector1< Size > cutpoint_open_in_full_model = option[ full_model::cutpoint_open ]();
		vector1< Size > extra_minimize_res = option[ full_model::extra_min_res ]();
		vector1< Size > input_res_list = option[ in::file::input_res ]();
		bool const get_res_list_from_pdb = !option[ in::file::input_res ].user();
		vector1< Size > sample_res = option[ full_model::sample_res ](); //stuff that can be resampled.
		//		if ( option[ stepwise::rna::global_sample_res_list ].user() )	sample_res = option[ stepwise::rna::global_sample_res_list ](); // legacy.
		//		vector1< Size > fixed_res = option[ stepwise::fixed_res ](); //legacy -- stuff that cannot be resampled.

		vector1< vector1< Size > > pose_res_lists;
		vector1< Size > domain_map( desired_sequence.size(), 0 );
		Size input_res_count( 0 );

		for ( Size n = 1; n <= pose_pointers.size(); n++ ) {

			Pose & pose = *pose_pointers[n];
			vector1< Size > input_res_for_pose;

			if ( get_res_list_from_pdb ){
				vector1< Size >  const res_list = get_res_num_from_pdb_info( pose );
				for ( Size n = 1; n <= res_list.size(); n++ ) input_res_list.push_back( res_list[n] );
			}

			for ( Size k = 1; k <= pose.total_residue(); k++ ){
				input_res_count++;
				runtime_assert( input_res_count <= input_res_list.size() );
				Size const & number_in_full_model = input_res_list[ input_res_count ];
				input_res_for_pose.push_back( number_in_full_model );
				//				if ( fixed_res.size() > 0 && !fixed_res.has_value( number_in_full_model ) && !sample_res.has_value( number_in_full_model ) )  sample_res.push_back( number_in_full_model ); // legacy -- if fixed_res is specified, as opposed to sample_res.
				if ( !sample_res.has_value( number_in_full_model ) ) domain_map[ number_in_full_model ] = n;
			}
			pose_res_lists.push_back( input_res_for_pose );
		}
		if ( input_res_count != input_res_list.size() ) utility_exit_with_message( "input_res size does not match pose size" );
		runtime_assert( input_res_count == input_res_list.size() );

		// check for extra cutpoint open.
		for ( Size n = 1; n <= pose_pointers.size(); n++ ) {
			Pose & pose = *pose_pointers[n];
			vector1< Size > const & res_list = pose_res_lists[ n ];
			for ( Size i = 1; i < pose.total_residue(); i++ ){
				if ( (res_list[ i+1 ] > res_list[ i ] + 1) && !pose.fold_tree().is_cutpoint(i) ){
					//					TR << "Adding jump between non-contiguous residues [in full model numbering]: " <<
					//						res_list[i] << " and " << res_list[ i+1 ] << std::endl;
					put_in_cutpoint( pose, i );
				}
				if ( cutpoint_open_in_full_model.has_value( res_list[ i ]) ) continue;
				if ( (res_list[ i+1 ] == res_list[ i ] + 1) &&
						 pose.fold_tree().is_cutpoint( i ) ){
					TR << "There appears to be a strand boundary at " << res_list[ i ] << " so adding to cutpoint_in_full_model." << std::endl;
					cutpoint_open_in_full_model.push_back( res_list[ i ] ); continue;
				}
				if ( pose.residue_type( i ).is_RNA() != pose.residue_type( i+1 ).is_RNA() ) {
					cutpoint_open_in_full_model.push_back( res_list[ i ] ); continue;
				}
				if ( pose.residue_type( i ).is_protein() != pose.residue_type( i+1 ).is_protein() ) {
					cutpoint_open_in_full_model.push_back( res_list[ i ] ); continue;
				}
			}
			add_cutpoint_closed( pose, res_list, option[ full_model::cutpoint_closed ]()  );
			update_pose_fold_tree( pose, res_list,
														 extra_minimize_res, sample_res,
														 option[ full_model::jump_res ](), option[ full_model::root_res ]() );
			add_virtual_sugar_res( pose, res_list, option[ full_model::virtual_sugar_res ]() );
		}


		full_model_parameters->set_parameter( FIXED_DOMAIN,  domain_map );
		full_model_parameters->set_parameter_as_res_list( CUTPOINT_OPEN, cutpoint_open_in_full_model );
		full_model_parameters->set_parameter_as_res_list( EXTRA_MINIMIZE, extra_minimize_res );
		full_model_parameters->set_parameter_as_res_list( SAMPLE, sample_res );
		full_model_parameters->set_parameter_as_res_list( CALC_RMS, option[ full_model::calc_rms_res ]() );
		full_model_parameters->set_parameter_as_res_list( RNA_SYN_CHI,  option[ full_model::rna::force_syn_chi_res_list ]() );
		full_model_parameters->set_parameter_as_res_list( RNA_TERMINAL, option[ full_model::rna::terminal_res ]() );

		for ( Size n = 1; n <= pose_pointers.size(); n++ ) {
			Pose & pose = *pose_pointers[n];
			FullModelInfoOP full_model_info_for_pose = new FullModelInfo( full_model_parameters );
			full_model_info_for_pose->set_res_list( pose_res_lists[ n ] );
			pose.data().set( core::pose::datacache::CacheableDataType::FULL_MODEL_INFO, full_model_info_for_pose );
			update_pdb_info_from_full_model_info( pose ); // for output pdb or silent file -- residue numbering.
		}

	}

	////////////////////////////////////////////////////////////////////////////////////
	void
	update_pose_fold_tree( pose::Pose & pose,
												 vector1< Size > const & res_list,
												 vector1< Size > const & extra_min_res,
												 vector1< Size > const & sample_res,
												 vector1< Size > const & jump_res,
												 vector1< Size > const & root_res ){

		if ( pose.total_residue() == 0 ) return;

		vector1< vector1< Size > > all_res_in_chain, all_fixed_res_in_chain;
		vector1< Size > moveable_res = sample_res;
		for ( Size n = 1; n <= extra_min_res.size(); n++ ) moveable_res.push_back( extra_min_res[n] );
		define_chains( pose, all_res_in_chain, all_fixed_res_in_chain, res_list, moveable_res );
		Size nchains = all_res_in_chain.size();

		vector1< Size > jump_partners1, jump_partners2, cuts, blank_vector;
		vector1< vector1< bool > > chain_connected;
		for ( Size i = 1; i <= nchains; i++ ) blank_vector.push_back( false );
		for ( Size i = 1; i <= nchains; i++ ) chain_connected.push_back( blank_vector );
		setup_user_defined_jumps( jump_res, jump_partners1, jump_partners2,
															chain_connected, res_list, all_res_in_chain );
		runtime_assert( jump_partners1.size() < nchains );

		setup_jumps( jump_partners1, jump_partners2, chain_connected, all_fixed_res_in_chain );
		setup_jumps( jump_partners1, jump_partners2, chain_connected, all_res_in_chain );
		runtime_assert( jump_partners1.size() == (nchains - 1) );

		for ( Size n = 1; n < nchains; n++ ) cuts.push_back( all_res_in_chain[n][ all_res_in_chain[n].size() ] );
		FoldTree f = get_tree( pose, cuts, jump_partners1, jump_partners2 );

		reroot( f, res_list, root_res );

		pose.fold_tree( f );

	}

	///////////////////////////////////////////////////////////////////////////////////////
	void
	define_chains( pose::Pose const & pose,
								 vector1< vector1< Size > > & all_res_in_chain,
								 vector1< vector1< Size > > & all_fixed_res_in_chain,
								 vector1< Size > const & res_list,
								 vector1< Size > const & moveable_res ){

		Size chain_start( 1 ), chain_end( 0 );
		for ( Size n = 1; n <= pose.total_residue(); n++ ){
			if ( !pose.fold_tree().is_cutpoint( n ) &&
					 n < pose.total_residue() ) continue;
			chain_end = n;
			vector1< Size > res_in_chain, fixed_res_in_chain;
			for ( Size i = chain_start; i <= chain_end; i++ ){
				res_in_chain.push_back( i );
				if ( !moveable_res.has_value( res_list[ i ] ) ) fixed_res_in_chain.push_back( i );
			}
			all_res_in_chain.push_back( res_in_chain );
			all_fixed_res_in_chain.push_back( fixed_res_in_chain );
			chain_start = chain_end + 1;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void
	setup_user_defined_jumps( vector1< Size > const & jump_res,
														vector1< Size > & jump_partners1,
														vector1< Size > & jump_partners2,
														vector1< vector1< bool > > & chain_connected,
														vector1< Size > const & res_list,
														vector1< vector1< Size > > const & all_res_in_chain ){
				// Figure out jump res.
		for ( Size n = 1; n <= jump_res.size()/2; n++ ){
			if ( res_list.has_value( jump_res[ 2*n - 1 ] )  &&
					 res_list.has_value( jump_res[ 2*n ] ) ){
				Size const i = res_list.index( jump_res[ 2*n - 1 ] );
				Size const j = res_list.index( jump_res[ 2*n ] );
				jump_partners1.push_back( i );
				jump_partners2.push_back( j );
				Size const chain_i( get_chain( i, all_res_in_chain ) );
				Size const chain_j( get_chain( j, all_res_in_chain ) );
				runtime_assert( !chain_connected[ chain_i ][ chain_j ] );
				chain_connected[ chain_i ][ chain_j ] = true;
			}
		}

	}

	////////////////////////////////////////////////////////////////////////////////
	Size
	get_chain( Size const i, vector1< vector1< Size > > const & all_res_in_chain ){
		for ( Size n = 1; n <= all_res_in_chain.size(); n++ ){
			if ( all_res_in_chain[ n ].has_value( i ) ) return n;
		}
		return 0;
	}


	////////////////////////////////////////////////////////////////////////////////
	void
	setup_jumps( vector1< Size > & jump_partners1,
							 vector1< Size > & jump_partners2,
							 vector1< vector1< bool > > & chain_connected,
							 vector1< vector1< Size > > const & all_res_in_chain ){
		Size const num_chains = all_res_in_chain.size();
		if ( jump_partners1.size() == num_chains - 1 ) return;
		for ( Size i = 1; i <= num_chains; i++ ){
			for ( Size j = (i+1); j <= num_chains; j++ ){
				if ( chain_connected[ i ][ j ] ) continue;
				vector1< Size > const & fixed_res_in_chain_i = all_res_in_chain[ i ];
				vector1< Size > const & fixed_res_in_chain_j = all_res_in_chain[ j ];
				if ( fixed_res_in_chain_i.size() > 0 &&
						 fixed_res_in_chain_j.size() > 0 ){
					jump_partners1.push_back( fixed_res_in_chain_i[ fixed_res_in_chain_i.size() ] );
					jump_partners2.push_back( fixed_res_in_chain_j[ 1 ] );
					chain_connected[ i ][ j ] = true;
				}
				if ( jump_partners1.size() == num_chains - 1 ) break;
			}
			if ( jump_partners1.size() == num_chains - 1 ) break;
		}
	}


	/////////////////////////////////////////////////////////////////////////////////
	FoldTree
	get_tree( pose::Pose const & pose,
						vector1< Size > const & cuts,
						vector1< Size > const & jump_partners1,
						vector1< Size > const & jump_partners2 ) {

		vector1< std::string > jump_atoms1, jump_atoms2;
		for ( Size n = 1; n <= jump_partners1.size(); n++ ){
			jump_atoms1.push_back( chemical::rna::default_jump_atom( pose.residue( jump_partners1[n] ) ) );
			jump_atoms2.push_back( chemical::rna::default_jump_atom( pose.residue( jump_partners2[n] ) ) );
		}
		return get_tree( pose.total_residue(), cuts, jump_partners1, jump_partners2, jump_atoms1, jump_atoms2 );
	}


	/////////////////////////////////////////////////////////////////////////////////
	FoldTree
	get_tree( Size const nres,
						vector1< Size > const & cuts,
						vector1< Size > const & jump_partners1,
						vector1< Size > const & jump_partners2,
						vector1< std::string > const & jump_atoms1,
						vector1< std::string > const & jump_atoms2 ) {

		Size const num_cuts = cuts.size();

		FoldTree f;
		ObjexxFCL::FArray2D< int > jump_point_( 2, num_cuts, 0 );
		ObjexxFCL::FArray1D< int > cuts_( num_cuts, 0 );
		for ( Size i = 1; i <= num_cuts; i++ ) {
			jump_point_( 1, i ) = std::min( jump_partners1[ i ], jump_partners2[ i ] );
			jump_point_( 2, i ) = std::max( jump_partners1[ i ], jump_partners2[ i ] );
			cuts_( i ) = cuts[ i ];
		}
		f.tree_from_jumps_and_cuts( nres, num_cuts, jump_point_, cuts_ );

		bool const KeepStubInResidue( true );
		for ( Size i = 1; i <= num_cuts; i++ ){
			Size const n = f.jump_nr( jump_partners1[ i ], jump_partners2[ i ] );
			f.set_jump_atoms( n,
												jump_partners1[ i ], jump_atoms1[ i ],
												jump_partners2[ i ], jump_atoms2[ i ], KeepStubInResidue );
		}
		f.reassign_atoms_for_intra_residue_stubs(); // it seems silly that we need to do this separately.

		return f;
	}

	/////////////////////////////////////////////////////////////////////////////////////
	void
	reroot( FoldTree & f, vector1< Size > const & res_list, vector1< Size > const & root_res ){

		Size new_root_res( 0 );
		for ( Size n = 1; n <= root_res.size(); n++ ) {
			if ( res_list.has_value( root_res[n] ) ){
				Size const i = res_list.index( root_res[n] );
				if ( f.possible_root( i ) ){
					runtime_assert( new_root_res == 0 );
					new_root_res = i;
				}
			}
		}
		if ( new_root_res > 0 ) f.reorder( new_root_res );
	}

	/////////////////////////////////////////////////////////////////////////////////////
	void
	add_cutpoint_closed( pose::Pose & pose,
											 vector1< Size > const & res_list,
											 vector1< Size > const & cutpoint_closed ){
		for ( Size n = 1; n <= cutpoint_closed.size(); n++ ){
			if ( !res_list.has_value( cutpoint_closed[ n ] ) ) continue;
			Size const i = res_list.index( cutpoint_closed[ n ] );
			put_in_cutpoint( pose, i );
			correctly_add_cutpoint_variants( pose, i );
		}
	}


	/////////////////////////////////////////////////////////////////////////////////////
	void
	put_in_cutpoint( pose::Pose & pose, Size const i ){
		core::kinematics::FoldTree f = pose.fold_tree();
		Size const new_jump = f.new_jump( i, i+1, i );
		if ( pose.residue_type( i ).is_RNA() && pose.residue_type( i+1 ).is_RNA() ){
			f.set_jump_atoms( new_jump,
												chemical::rna::default_jump_atom( pose.residue( f.upstream_jump_residue( new_jump ) ) ),
												chemical::rna::default_jump_atom( pose.residue( f.downstream_jump_residue( new_jump ) ) ) );
		}
		pose.fold_tree( f );
	}

	/////////////////////////////////////////////////////////////////////////////////////
	void
	add_virtual_sugar_res( pose::Pose & pose,
												 vector1< Size > const & res_list,
												 vector1< Size > const & virtual_sugar_res ){
		for ( Size n = 1; n <= virtual_sugar_res.size(); n++ ){
			if ( !res_list.has_value( virtual_sugar_res[ n ] ) ) continue;
			Size const i = res_list.index( virtual_sugar_res[ n ] );
			runtime_assert( i == 1 || pose.fold_tree().is_cutpoint( i - 1 ) );
			runtime_assert( i == pose.total_residue() || pose.fold_tree().is_cutpoint( i ) );
			add_variant_type_to_pose_residue( pose, "VIRTUAL_RIBOSE", i );
		}
	}

} //full_model_info
} //pose
} //core
