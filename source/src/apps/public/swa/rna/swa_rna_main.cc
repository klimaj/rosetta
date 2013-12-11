// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.
/// @file swa_rna_main.cc
/// @author Parin Sripakdeevong (sripakpa@stanford.edu), Rhiju Das (rhiju@stanford.edu)

// libRosetta headers
#include <protocols/swa/rna/StepWiseRNA_PoseSetupFromCommandLine.hh>
#include <protocols/swa/rna/StepWiseRNA_Modeler.hh>
#include <core/types.hh>
#include <core/chemical/AA.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/ResidueSelector.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/util.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/conformation/ResidueMatcher.hh>
#include <core/conformation/Conformation.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/chemical/rna/RNA_FittedTorsionInfo.hh>
#include <core/chemical/rna/RNA_Util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/EnergyGraph.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/EnergyMap.hh>
#include <core/scoring/EnergyMap.fwd.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/func/CharmmPeriodicFunc.hh>
#include <core/scoring/constraints/DihedralConstraint.hh>

#include <core/sequence/util.hh>
#include <core/sequence/Sequence.hh>

#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/tree/Atom.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/id/AtomID_Map.hh>
#include <core/id/AtomID.hh>
#include <core/id/DOF_ID.hh>
#include <core/init/init.hh>


#include <utility/file/file_sys_util.hh> //Add by Parin on May 04, 2011.

//////////////////////////////////////////////////
#include <basic/options/keys/swa.OptionKeys.gen.hh>
#include <basic/options/keys/rna.OptionKeys.gen.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <basic/options/keys/edensity.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh> // for option[ out::file::silent  ] and etc.
#include <basic/options/keys/in.OptionKeys.gen.hh> // for option[ in::file::tags ] and etc.
#include <basic/options/keys/OptionKeys.hh>
#include <basic/options/option_macros.hh>
#include <basic/Tracer.hh>
///////////////////////////////////////////////////
#include <protocols/idealize/idealize.hh>

#include <core/optimization/AtomTreeMinimizer.hh>
#include <core/optimization/MinimizerOptions.hh>

#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>

#include <utility/vector1.hh>
#include <utility/io/ozstream.hh>
#include <utility/io/izstream.hh>
#include <utility/excn/Exceptions.hh>
#include <numeric/xyzVector.hh>
#include <numeric/conversions.hh>

#include <core/pose/MiniPose.hh>
#include <core/pose/util.hh>

#include <core/io/pdb/pose_io.hh>
#include <core/io/silent/SilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/io/silent/util.hh>
#include <core/import_pose/pose_stream/SilentFilePoseInputStream.hh>
#include <core/import_pose/import_pose.hh>

//////////////////////////////////////////////////////////
#include <protocols/viewer/viewers.hh>
#include <protocols/rna/RNA_ProtocolUtil.hh>
#include <protocols/rna/RNA_LoopCloser.hh>
#include <protocols/swa/rna/StepWiseRNA_OutputData.hh>
#include <protocols/swa/rna/StepWiseRNA_CombineLongLoopFilterer.hh>
#include <protocols/swa/rna/StepWiseRNA_CombineLongLoopFilterer.fwd.hh>
#include <protocols/swa/rna/StepWiseRNA_VirtualSugarSamplerFromStringList.hh>
#include <protocols/swa/rna/StepWiseRNA_Minimizer.hh>
#include <protocols/swa/rna/StepWiseRNA_ResidueSampler.hh>
#include <protocols/swa/rna/StepWiseRNA_PoseSetup.fwd.hh>
#include <protocols/swa/rna/StepWiseRNA_PoseSetup.hh>
#include <protocols/swa/rna/StepWiseRNA_JobParametersSetup.hh>
#include <protocols/swa/rna/StepWiseRNA_JobParameters.hh>
#include <protocols/swa/rna/StepWiseRNA_Clusterer.hh>
#include <protocols/swa/rna/screener/StepWiseRNA_VDW_BinScreener.hh>
#include <protocols/swa/rna/screener/StepWiseRNA_BaseCentroidScreener.hh>
#include <ObjexxFCL/string.functions.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/FArray1D.hh>
#include <ObjexxFCL/FArray2D.hh>

// C++ headers
//#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>  //Test
#include <cctype>
#include <iomanip>
#include <map>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <libgen.h>
#define GetCurrentDir getcwd

#include <list>
#include <stdio.h>
#include <math.h>

using namespace core;
using namespace protocols;
using namespace ObjexxFCL;
using namespace basic::options;
using namespace basic::options::OptionKeys;
using utility::vector1;
using io::pdb::dump_pdb;

typedef  numeric::xyzMatrix< Real > Matrix;

static basic::Tracer TR( "swa_rna_main" );

OPT_KEY( Boolean, do_not_sample_multiple_virtual_sugar )
OPT_KEY( Boolean, sample_ONLY_multiple_virtual_sugar )
OPT_KEY( Boolean, skip_sampling )
OPT_KEY( Boolean, skip_clustering )
OPT_KEY( Boolean, minimize_and_score_native_pose )
OPT_KEY( Integer, num_pose_minimize )
OPT_KEY( Boolean, filter_for_previous_contact )
OPT_KEY( Boolean, filter_for_previous_clash )
OPT_KEY( Boolean, filterer_undercount_sugar_rotamers )
OPT_KEY( Boolean, sampler_include_torsion_value_in_tag )
OPT_KEY( Boolean, sampler_extra_chi_rotamer )
OPT_KEY( Boolean, sampler_extra_beta_rotamer )
OPT_KEY( Boolean, sampler_extra_epsilon_rotamer )
OPT_KEY( Boolean, sample_both_sugar_base_rotamer )
OPT_KEY( Boolean, reinitialize_CCD_torsions )
OPT_KEY( Boolean, PBP_clustering_at_chain_closure )
OPT_KEY( Boolean, clusterer_two_stage_clustering )
OPT_KEY( Boolean, clusterer_keep_pose_in_memory )
OPT_KEY( Boolean, finer_sampling_at_chain_closure )
OPT_KEY( StringVector, 	VDW_rep_screen_info )
OPT_KEY( Real, 	VDW_rep_alignment_RMSD_CUTOFF )
OPT_KEY( Boolean, graphic )
OPT_KEY( Real, Real_parameter_one )
OPT_KEY( Boolean, clusterer_quick_alignment )
OPT_KEY( Boolean, clusterer_align_only_over_base_atoms )
OPT_KEY( Boolean, clusterer_optimize_memory_usage )
OPT_KEY( Integer, clusterer_min_struct )
OPT_KEY( Boolean, clusterer_write_score_only )
OPT_KEY( Boolean, add_lead_zero_to_tag )
OPT_KEY( Boolean, distinguish_pucker )
OPT_KEY( Boolean, sampler_allow_syn_pyrimidine )
OPT_KEY( Boolean, erraser )
OPT_KEY( Boolean, virtual_sugar_legacy_mode )
OPT_KEY( Real, whole_struct_cluster_radius )
OPT_KEY( Real, suite_cluster_radius )
OPT_KEY( Real, loop_cluster_radius )
OPT_KEY( Boolean, parin_favorite_output )
OPT_KEY( Boolean, centroid_screen )
OPT_KEY( Boolean, allow_base_pair_only_centroid_screen )
OPT_KEY( Boolean, VDW_atr_rep_screen )
OPT_KEY( Boolean, sampler_perform_o2prime_pack )
OPT_KEY( Boolean, allow_bulge_at_chainbreak )
OPT_KEY( Boolean, VERBOSE )
OPT_KEY( Boolean, sampler_native_rmsd_screen )
OPT_KEY( Real, sampler_native_screen_rmsd_cutoff )
OPT_KEY( Real, native_edensity_score_cutoff )
OPT_KEY( Real, score_diff_cut )
OPT_KEY( Boolean, clusterer_perform_score_diff_cut )
OPT_KEY( String, 	algorithm )
OPT_KEY( Integer, sampler_num_pose_kept )
OPT_KEY( Integer, clusterer_num_pose_kept )
OPT_KEY( Boolean, recreate_silent_struct )
OPT_KEY( Boolean, clusterer_use_best_neighboring_shift_RMSD )
OPT_KEY( Boolean, clusterer_rename_tags )
OPT_KEY( Boolean, minimizer_perform_o2prime_pack )
OPT_KEY( Boolean, minimizer_output_before_o2prime_pack )
OPT_KEY( Boolean, minimizer_rename_tag )
OPT_KEY( Boolean, minimizer_perform_minimize )
OPT_KEY( StringVector, 	VDW_rep_delete_matching_res )
OPT_KEY( Boolean, clusterer_perform_VDW_rep_screen )
OPT_KEY( Boolean, clusterer_perform_filters )
OPT_KEY( Integer, clusterer_min_num_south_sugar_filter )
OPT_KEY( Real,  VDW_rep_screen_physical_pose_clash_dist_cutoff )
OPT_KEY( Boolean,  clusterer_full_length_loop_rmsd_clustering )
OPT_KEY( StringVector, 	sample_virtual_sugar_list )
OPT_KEY( Boolean,  sampler_assert_no_virt_sugar_sampling )
OPT_KEY( Boolean,  sampler_try_sugar_instantiation )
OPT_KEY( Boolean,  clusterer_ignore_FARFAR_no_auto_bulge_tag )
OPT_KEY( Boolean,  clusterer_ignore_FARFAR_no_auto_bulge_parent_tag )
OPT_KEY( Boolean,  clusterer_ignore_unmatched_virtual_res )
OPT_KEY( String, 	start_silent )
OPT_KEY( String, 	start_tag )
OPT_KEY( Boolean,  simple_full_length_job_params )
OPT_KEY( Real, sampler_cluster_rmsd )
OPT_KEY( Boolean, 	integration_test )
OPT_KEY ( Boolean, constraint_chi )
OPT_KEY ( Boolean, rm_virt_phosphate )

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
swa_rna_sample()
{

  using namespace core::pose;
  using namespace core::chemical;
  using namespace core::kinematics;
  using namespace core::scoring;
	using namespace protocols::swa::rna;

	output_title_text( "Enter swa_rna_sample()", TR );

  Pose pose;

	StepWiseRNA_JobParametersOP	job_parameters = setup_rna_job_parameters( true  /*check_for_previously_closed_cutpoint_with_input_pose */ );
	StepWiseRNA_JobParametersCOP job_parameters_COP( job_parameters );
	StepWiseRNA_PoseSetupOP stepwise_rna_pose_setup = setup_pose_setup_class( job_parameters );
	stepwise_rna_pose_setup->apply( pose );
	stepwise_rna_pose_setup->setup_native_pose( pose ); //NEED pose to align native_pose to pose.
	PoseCOP native_pose = job_parameters_COP->working_native_pose();

	if ( option[ graphic ]() ) protocols::viewer::add_conformation_viewer ( pose.conformation(), get_working_directory(), 400, 400 );

	core::scoring::ScoreFunctionOP scorefxn = create_scorefxn();
	// put following in pose setup?
	if ( option[ constraint_chi ]() )  apply_chi_cst( pose, *job_parameters_COP->working_native_pose() ); // should be in stepwise_rna_pose_setup.

	// Fang: The score term elec_dens_atomwise uses the first pose it scored to decide the normalization factor.
	// Can't this go inside ResidueSampler? -- rhiju
	if ( option[ in::file::native ].user() ) {
		( *scorefxn )( *(native_pose->clone()) );
	} else {
		( *scorefxn )( pose );
	}

	Size num_struct =  option[ out::nstruct ]();
	bool const multiple_shots = option[ out::nstruct ].user();
	std::string const silent_file = option[ out::file::silent ]();
	std::string swa_silent_file, out_tag;
	Pose start_pose = pose;

	for ( Size n = 1; n <= num_struct; n++ ){

		TR << TR.Blue << "Embarking on structure " << n << " of " << num_struct << TR.Reset << std::endl;

		pose = start_pose;

		if ( !get_tag_and_silent_file_for_struct( swa_silent_file, out_tag, n, multiple_shots, silent_file ) ) continue;

		Size const working_moving_res = job_parameters_COP->working_moving_res();

		StepWiseRNA_Modeler stepwise_rna_modeler( working_moving_res, scorefxn );

		stepwise_rna_modeler.set_native_pose( native_pose ); // wait put thi inside
		stepwise_rna_modeler.set_silent_file( swa_silent_file );
		stepwise_rna_modeler.set_sampler_num_pose_kept ( option[ sampler_num_pose_kept ]() );
		// following should probably be 'reference pose',  does not have to be native.
		stepwise_rna_modeler.set_sampler_native_rmsd_screen ( option[ sampler_native_rmsd_screen ]() );
		stepwise_rna_modeler.set_sampler_native_screen_rmsd_cutoff ( option[ sampler_native_screen_rmsd_cutoff ]() );
		stepwise_rna_modeler.set_o2prime_screen ( option[ sampler_perform_o2prime_pack ]() );
		stepwise_rna_modeler.set_verbose ( option[ VERBOSE ]() );
		stepwise_rna_modeler.set_cluster_rmsd (	option[ sampler_cluster_rmsd ]()	);
		stepwise_rna_modeler.set_distinguish_pucker ( option[ distinguish_pucker]() );
		stepwise_rna_modeler.set_finer_sampling_at_chain_closure ( option[ finer_sampling_at_chain_closure]() );
		stepwise_rna_modeler.set_PBP_clustering_at_chain_closure ( option[ PBP_clustering_at_chain_closure]() );
		stepwise_rna_modeler.set_allow_syn_pyrimidine( option[ sampler_allow_syn_pyrimidine ]() );
		stepwise_rna_modeler.set_extra_chi( option[ sampler_extra_chi_rotamer]() );
		stepwise_rna_modeler.set_use_phenix_geo ( option[ basic::options::OptionKeys::rna::corrected_geo ]() );
		stepwise_rna_modeler.set_kic_sampling( option[ erraser ]() );
		stepwise_rna_modeler.set_centroid_screen ( option[ centroid_screen ]() );
		stepwise_rna_modeler.set_VDW_atr_rep_screen ( option[ VDW_atr_rep_screen ]() );
		stepwise_rna_modeler.set_VDW_rep_screen_info ( option[ VDW_rep_screen_info ]() );
		stepwise_rna_modeler.set_VDW_rep_alignment_RMSD_CUTOFF ( option[ VDW_rep_alignment_RMSD_CUTOFF ]() );
		stepwise_rna_modeler.set_choose_random( option[ basic::options::OptionKeys::swa::rna::choose_random ]()  );
		stepwise_rna_modeler.set_num_random_samples( option[ basic::options::OptionKeys::swa::rna::num_random_samples ]() );
		stepwise_rna_modeler.set_skip_sampling( option[ skip_sampling ]() );
		stepwise_rna_modeler.set_perform_minimize( option[ minimizer_perform_minimize ]() );
		stepwise_rna_modeler.set_native_edensity_score_cutoff ( option[native_edensity_score_cutoff]() );
		stepwise_rna_modeler.set_rm_virt_phosphate ( option[rm_virt_phosphate]() );
		stepwise_rna_modeler.set_minimize_and_score_sugar ( option[ basic::options::OptionKeys::swa::rna::minimize_and_score_sugar ]() );
		stepwise_rna_modeler.set_minimize_and_score_native_pose ( option[ minimize_and_score_native_pose ]() );
		if ( option[ basic::options::OptionKeys::swa::rna::choose_random ]() ) stepwise_rna_modeler.set_num_pose_minimize( 1 );
		if ( option[ num_pose_minimize ].user() ) stepwise_rna_modeler.set_num_pose_minimize( option[ num_pose_minimize ]() );
		stepwise_rna_modeler.set_output_minimized_pose_list( !multiple_shots );
		stepwise_rna_modeler.set_virtual_sugar_legacy_mode( option[ virtual_sugar_legacy_mode ] );
		stepwise_rna_modeler.set_virtual_sugar_legacy_mode( option[ virtual_sugar_legacy_mode ] );

		// newer options
		stepwise_rna_modeler.set_VDW_rep_delete_matching_res ( option[ VDW_rep_delete_matching_res ]() );
		stepwise_rna_modeler.set_VDW_rep_screen_physical_pose_clash_dist_cutoff ( option[ VDW_rep_screen_physical_pose_clash_dist_cutoff ]() );
		stepwise_rna_modeler.set_integration_test_mode( option[ integration_test ]() ); //Should set after setting sampler_native_screen_rmsd_cutoff, fast, medium_fast options.
		stepwise_rna_modeler.set_allow_bulge_at_chainbreak( option[ allow_bulge_at_chainbreak ]() );
		stepwise_rna_modeler.set_parin_favorite_output( option[ parin_favorite_output ]() );
		stepwise_rna_modeler.set_reinitialize_CCD_torsions( option[ reinitialize_CCD_torsions]() );
		stepwise_rna_modeler.set_sampler_extra_epsilon_rotamer( option[ sampler_extra_epsilon_rotamer]() );
		stepwise_rna_modeler.set_sampler_extra_beta_rotamer( option[ sampler_extra_beta_rotamer]() );
		stepwise_rna_modeler.set_sample_both_sugar_base_rotamer( option[ sample_both_sugar_base_rotamer]() ); //Nov 12, 2010
		stepwise_rna_modeler.set_sampler_include_torsion_value_in_tag( option[ sampler_include_torsion_value_in_tag]() );
		stepwise_rna_modeler.set_combine_long_loop_mode( option[ basic::options::OptionKeys::swa::rna::combine_long_loop_mode]() );
		stepwise_rna_modeler.set_do_not_sample_multiple_virtual_sugar( option[ do_not_sample_multiple_virtual_sugar]() );
		stepwise_rna_modeler.set_sample_ONLY_multiple_virtual_sugar( option[ sample_ONLY_multiple_virtual_sugar]() );
		stepwise_rna_modeler.set_sampler_assert_no_virt_sugar_sampling( option[ sampler_assert_no_virt_sugar_sampling ]() );
		stepwise_rna_modeler.set_sampler_try_sugar_instantiation( option[ sampler_try_sugar_instantiation ]() );
		stepwise_rna_modeler.set_allow_base_pair_only_centroid_screen( option[ allow_base_pair_only_centroid_screen ]() );

		stepwise_rna_modeler.set_minimizer_perform_o2prime_pack( option[ minimizer_perform_o2prime_pack ]() );
		stepwise_rna_modeler.set_minimizer_output_before_o2prime_pack( option[ minimizer_output_before_o2prime_pack ]() );
		stepwise_rna_modeler.set_minimizer_rename_tag( option[ minimizer_rename_tag ]() );

		// turn this on to test StepWiseRNA_Modeler's "on-the-fly" determination of job based on pose fold_tree, cutpoints, etc.
		//  in this case, need to tell stepwise_rna_modeler some additional arbitrary information that cannot be determined from
		//  info sitting inside the pose -- fixed_res & rmsd_res (over which to calculate rmsds).
		if ( !option[ basic::options::OptionKeys::swa::rna::test_encapsulation ]() )	stepwise_rna_modeler.set_job_parameters( job_parameters );
		stepwise_rna_modeler.set_fixed_res ( job_parameters->working_fixed_res() );
		stepwise_rna_modeler.set_rmsd_res_list ( job_parameters->rmsd_res_list() /*global numbering*/ );

		// in traditional swa, this creates silent file
		stepwise_rna_modeler.apply( pose );

		// instead we should be able to output those silent structs if we want them. Here outputting best scoring pose, not whatever comes out randomly.
		if ( multiple_shots ) stepwise_rna_modeler.output_pose( pose, out_tag, silent_file );

	}

}


///////////////////////////////////////////////////////////////
void
swa_rna_cluster(){

	using namespace protocols::swa::rna;

	StepWiseRNA_JobParametersOP job_parameters;

	bool job_parameters_exist = false;
	if ( option[ simple_full_length_job_params ]() ){ //Oct 31, 2011.
		std::cout << "USING simple_full_length_job_params!" << std::endl;
		job_parameters_exist = true;
		job_parameters = setup_simple_full_length_rna_job_parameters();

	} else if ( option[ basic::options::OptionKeys::swa::rna::rmsd_res ].user() ){
		job_parameters_exist = true;
		job_parameters = setup_rna_job_parameters();
		print_JobParameters_info( job_parameters, "standard_clusterer_job_params", TR, false /*is_simple_full_length_JP*/ );
	}

	StepWiseRNA_JobParametersCOP job_parameters_COP = job_parameters;
	//////////////////////////////////////////////////////////////
	screener::StepWiseRNA_VDW_BinScreenerOP user_input_VDW_bin_screener = new screener::StepWiseRNA_VDW_BinScreener();
	if ( option[ VDW_rep_screen_info].user() ){ //This is used for post_processing only. Main VDW_rep_screener should be in the sampler.
		user_input_VDW_bin_screener->set_VDW_rep_alignment_RMSD_CUTOFF( option[ VDW_rep_alignment_RMSD_CUTOFF]() );
		user_input_VDW_bin_screener->set_VDW_rep_delete_matching_res( option[ VDW_rep_delete_matching_res ]() );
		user_input_VDW_bin_screener->set_physical_pose_clash_dist_cutoff( option[ VDW_rep_screen_physical_pose_clash_dist_cutoff ]() );
	}

	//////////////////////////////////////////////////////////////

	utility::vector1< std::string > const silent_files_in( option[ in::file::silent ]() );
	utility::vector1< std::string > non_empty_silent_files_in;
	non_empty_silent_files_in.clear();

	/////////////Check for empty silent_files/////////////////
	for ( Size n = 1; n <= silent_files_in.size(); n++ ){

		std::string const input_silent_file = silent_files_in[n];

		if ( is_nonempty_input_silent_file( input_silent_file, "empty filtered silent_file since no non - empty sampler silent_file." ) ){
			std::cout << "adding input_silent_file " << input_silent_file << " to non_empty_silent_files_in " << std::endl;
			non_empty_silent_files_in.push_back( input_silent_file );
		}

	}
	//////////////////////////////////////////////////
	std::string const silent_file_out = option[ out::file::silent  ]();

	////////////////////////////////////////////////////////////////////////////////
	//May 04, 2011. Make sure that silent_file_out doesn't exist before the clustering process.
	//Mainly need this becuase the BIOX2-cluster (Stanford) is not robust...The node containing a slave_job can suddenly become unaviable and slave_job will need to be resubmitted to a new node.
	//For example:
	//		Your job <90107> has been killed because the execution host <node-9-30> is no longer available.
	//		The job will be re-queued and re-run with the same jobId.

	if ( utility::file::file_exists( silent_file_out ) ) {
		std::cout << "WARNING: silent_file_out " << silent_file_out << " already exist! removing..." << std::endl;
		int remove_file_return_value = std::remove( silent_file_out.c_str() );
		std::cout << "remove_file_return_value = " <<  remove_file_return_value << " for std::remove( " << silent_file_out << " )" << std::endl;
		runtime_assert( remove_file_return_value == 0 );
	}

	////////////////////////////////////////////////////////////////////////////////
	if ( non_empty_silent_files_in.size() == 0 ){
		std::cout << "Early Exit: non_empty_silent_files_in.size() == 0, outputting empty clustered outfile " << std::endl;
		std::ofstream outfile;
		outfile.open( silent_file_out.c_str() ); //Opening the file with this command removes all prior content..
		outfile << "empty cluster silent_file since all input_silent_file are empty.";
		outfile << " input_silent_file:";
		for ( Size n = 1; n <= silent_files_in.size(); n++ ){
			outfile << " " << silent_files_in[n];
		}
		outfile << "\n";
		outfile.flush();
		outfile.close();
		return; //Early return;
	}


	//////////////////////////////////////////////////
	protocols::swa::rna::StepWiseRNA_Clusterer stepwise_rna_clusterer( non_empty_silent_files_in );

	stepwise_rna_clusterer.set_max_decoys( option[ clusterer_num_pose_kept ]() );
	stepwise_rna_clusterer.set_score_diff_cut( option[ score_diff_cut ]() );
	stepwise_rna_clusterer.set_perform_score_diff_cut( option[ clusterer_perform_score_diff_cut ] );
	stepwise_rna_clusterer.set_cluster_radius(	option[ whole_struct_cluster_radius ]()	);
	stepwise_rna_clusterer.set_rename_tags( option[ clusterer_rename_tags ]() );
	stepwise_rna_clusterer.set_job_parameters( job_parameters_COP );
	stepwise_rna_clusterer.set_job_parameters_exist( job_parameters_exist );
	stepwise_rna_clusterer.set_suite_cluster_radius( option[ suite_cluster_radius]() );
	stepwise_rna_clusterer.set_loop_cluster_radius( option[ loop_cluster_radius]() );
	stepwise_rna_clusterer.set_distinguish_pucker( option[ distinguish_pucker]() );
	stepwise_rna_clusterer.set_add_lead_zero_to_tag( option[ add_lead_zero_to_tag ]() );
	stepwise_rna_clusterer.set_quick_alignment( option[ clusterer_quick_alignment ]() );
	stepwise_rna_clusterer.set_align_only_over_base_atoms( option[clusterer_align_only_over_base_atoms]() );
	stepwise_rna_clusterer.set_optimize_memory_usage( option [clusterer_optimize_memory_usage]() );
	stepwise_rna_clusterer.set_keep_pose_in_memory( option [clusterer_keep_pose_in_memory]() );
	stepwise_rna_clusterer.set_two_stage_clustering( option [clusterer_two_stage_clustering]() );
	stepwise_rna_clusterer.set_PBP_clustering_at_chain_closure( option[ PBP_clustering_at_chain_closure]() );
	stepwise_rna_clusterer.set_verbose( option[ VERBOSE ]() );
	stepwise_rna_clusterer.set_skip_clustering( option [skip_clustering]() );
	stepwise_rna_clusterer.set_full_length_loop_rmsd_clustering( option[clusterer_full_length_loop_rmsd_clustering]() );
	stepwise_rna_clusterer.set_filter_virtual_res_list( option[ basic::options::OptionKeys::swa::rna::virtual_res ]() );
	stepwise_rna_clusterer.set_perform_VDW_rep_screen( option[ clusterer_perform_VDW_rep_screen ]() );
	stepwise_rna_clusterer.set_perform_filters( option[ clusterer_perform_filters ]() );
	stepwise_rna_clusterer.set_min_num_south_sugar_filter( option[ clusterer_min_num_south_sugar_filter ]() );
	stepwise_rna_clusterer.set_VDW_rep_screen_info( option[ VDW_rep_screen_info]() );
	stepwise_rna_clusterer.set_user_input_VDW_bin_screener( user_input_VDW_bin_screener );
	stepwise_rna_clusterer.set_ignore_FARFAR_no_auto_bulge_tag( option[ clusterer_ignore_FARFAR_no_auto_bulge_tag ]() );
	stepwise_rna_clusterer.set_ignore_FARFAR_no_auto_bulge_parent_tag( option[ clusterer_ignore_FARFAR_no_auto_bulge_parent_tag ]() );
	stepwise_rna_clusterer.set_ignore_unmatched_virtual_res( option[ clusterer_ignore_unmatched_virtual_res ]() );
	stepwise_rna_clusterer.set_output_pdb( option[ basic::options::OptionKeys::swa::rna::output_pdb ]() );

	stepwise_rna_clusterer.cluster();


	bool const recreate_silent_struct_for_output = ( option[ recreate_silent_struct ]() );
	bool const use_best_neighboring_shift_RMSD_for_output = ( option[ clusterer_use_best_neighboring_shift_RMSD ]() );

	Size num_special_mode = 0;
	if ( recreate_silent_struct_for_output ) num_special_mode++;
	if ( use_best_neighboring_shift_RMSD_for_output ) num_special_mode++;
	if ( num_special_mode > 1 ) utility_exit_with_message( "num_special_mode( " + ObjexxFCL::string_of( num_special_mode ) + " ) > 1" );

	if ( recreate_silent_struct_for_output ){
		//For analysis purposes....for example rescore with a different force-field...change native_pose and etc..
		runtime_assert( job_parameters_exist );

		//core::scoring::ScoreFunctionOP scorefxn=create_scorefxn();
		StepWiseRNA_PoseSetupOP stepwise_rna_pose_setup = setup_pose_setup_class( job_parameters, false /*COPY DOF*/ ); //This contains the native_pose

		stepwise_rna_clusterer.recalculate_rmsd_and_output_silent_file( silent_file_out,
																														stepwise_rna_pose_setup,
																														option[clusterer_write_score_only]() );

	} else if ( use_best_neighboring_shift_RMSD_for_output ){

		runtime_assert ( job_parameters_exist );
		stepwise_rna_clusterer.get_best_neighboring_shift_RMSD_and_output_silent_file( silent_file_out );

	} else{ //default, just output existing silent_struct

		stepwise_rna_clusterer.output_silent_file( silent_file_out );

	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
rna_sample_virtual_sugar(){ //July 19th, 2011...rebuild the bulge nucleotides after floating base step to properly close the chain.

  using namespace core::pose;
  using namespace core::chemical;
  using namespace core::kinematics;
  using namespace core::scoring;
	using namespace protocols::swa::rna;

	output_title_text( "Enter rna_sample_virtual_sugar()", TR );

	ResidueTypeSetCAP rsd_set;
	rsd_set = core::chemical::ChemicalManager::get_instance()->residue_type_set( RNA );

	core::scoring::ScoreFunctionOP const scorefxn = create_scorefxn();

	StepWiseRNA_JobParametersOP	job_parameters = setup_rna_job_parameters( false );
	StepWiseRNA_JobParametersCOP job_parameters_COP( job_parameters );
	StepWiseRNA_PoseSetupOP stepwise_rna_pose_setup = setup_pose_setup_class( job_parameters, false /*COPY DOF*/ );

	utility::vector1< std::string > const sample_virtual_sugar_string_list = option[ sample_virtual_sugar_list ]();
	utility::vector1< std::string > input_tags;
	utility::vector1< std::string > silent_files_in;
	runtime_assert( option[ in::file::silent ].user() );
	silent_files_in = option[ in::file::silent ]();
	input_tags = get_silent_file_tags();

	runtime_assert( silent_files_in.size() == 1 );
	runtime_assert( silent_files_in.size() == input_tags.size() );

  pose::Pose pose;
	import_pose_from_silent_file( pose, silent_files_in[ 1 ], input_tags[1] );
	protocols::rna::assert_phosphate_nomenclature_matches_mini( pose );
	stepwise_rna_pose_setup->update_fold_tree_at_virtual_sugars( pose );

  std::string const silent_file_out = option[ out::file::silent  ]();
	if ( option[ graphic ]() ) protocols::viewer::add_conformation_viewer( pose.conformation(), get_working_directory(), 400, 400 );
	stepwise_rna_pose_setup->setup_native_pose( pose ); //NEED pose to align native_pose to pose.

	StepWiseRNA_VirtualSugarSamplerFromStringList virtual_sugar_sampler_from_string_list( job_parameters_COP, sample_virtual_sugar_string_list );
	virtual_sugar_sampler_from_string_list.set_scorefxn( scorefxn );
	virtual_sugar_sampler_from_string_list.set_silent_file_out( silent_file_out );
	virtual_sugar_sampler_from_string_list.set_tag( input_tags[1] );
	virtual_sugar_sampler_from_string_list.set_integration_test_mode( option[ integration_test ] );
	virtual_sugar_sampler_from_string_list.set_use_phenix_geo( option[ basic::options::OptionKeys::rna::corrected_geo ]() );
	virtual_sugar_sampler_from_string_list.set_legacy_mode( option[ virtual_sugar_legacy_mode ] );
	virtual_sugar_sampler_from_string_list.set_choose_random( option[ basic::options::OptionKeys::swa::rna::choose_random ] );
	virtual_sugar_sampler_from_string_list.apply( pose );

	output_title_text( "Exit rna_sample_virtual_sugar()", TR );

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void
filter_combine_long_loop()
{
  using namespace core::pose;
  using namespace core::chemical;
  using namespace core::kinematics;
  using namespace core::scoring;
	using namespace protocols::swa::rna;

	ResidueTypeSetCAP rsd_set;
	rsd_set = core::chemical::ChemicalManager::get_instance()->residue_type_set( RNA );

	///////////////////////////////
	StepWiseRNA_JobParametersOP	job_parameters = setup_rna_job_parameters( false );
	StepWiseRNA_JobParametersCOP job_parameters_COP( job_parameters );

	runtime_assert ( option[ in::file::silent ].user() );

	utility::vector1< std::string > const silent_files_in = option[ in::file::silent ]();
	std::string const output_filename = option[ OptionKeys::swa::rna::filter_output_filename ]();
	if ( utility::file::file_exists( output_filename ) ) { //Feb 08, 2012
		std::cout << "WARNING: output_filename " << output_filename << " already exists! removing..." << std::endl;
		int remove_file_return_value = std::remove( output_filename.c_str() );
		std::cout << "remove_file_return_value = " <<  remove_file_return_value << " for std::remove( " << output_filename << " )" << std::endl;
		runtime_assert ( remove_file_return_value == 0 );
	}

	runtime_assert( silent_files_in.size() == 2 );
	bool at_least_one_empty_silent_file = false;
	for ( Size n = 1; n <= 2; n++ ){
		std::string const input_silent_file = silent_files_in[n];
		if ( !is_nonempty_input_silent_file( input_silent_file, "empty cluster silent_file since all input_silent_file are empty." ) ){
			std::cout << input_silent_file << " is empty" << std::endl;
			at_least_one_empty_silent_file = true;
		}
	}
	if ( at_least_one_empty_silent_file ){
		std::cout << "Early Exit: since at_least_one_empty_silent_file, outputting empty filterer outfile " << std::endl;
		std::ofstream outfile;
		outfile.open( output_filename.c_str() ); //Opening the file with this command removes all prior content..
		outfile << "empty cluster silent_file since at least one of the two input_silent_file is empty.";
		outfile << " input_silent_file:";
		for ( Size n = 1; n <= silent_files_in.size(); n++ ){
			outfile << " " << silent_files_in[n];
		}
		outfile << "\n";
		outfile.flush();
		outfile.close();
		return; //Early return;
	}

	StepWiseRNA_CombineLongLoopFilterer stepwise_combine_long_loop_filterer( job_parameters_COP, option[basic::options::OptionKeys::swa::rna::combine_helical_silent_file] );
	stepwise_combine_long_loop_filterer.set_max_decoys( option[clusterer_num_pose_kept]() ); //Updated on Jan 12, 2012
	stepwise_combine_long_loop_filterer.set_silent_files_in( silent_files_in );
	stepwise_combine_long_loop_filterer.set_output_filename( output_filename );
	stepwise_combine_long_loop_filterer.set_filter_for_previous_contact( option[filter_for_previous_contact] );
	stepwise_combine_long_loop_filterer.set_filter_for_previous_clash( option[filter_for_previous_clash] );
	stepwise_combine_long_loop_filterer.set_undercount_sugar_rotamers( option[filterer_undercount_sugar_rotamers] );
	stepwise_combine_long_loop_filterer.set_parin_favorite_output( option[ parin_favorite_output ]() );

	stepwise_combine_long_loop_filterer.filter();
}

///////////////////////////////////////////////////////////////////////////////
// This code appears to take results of SWA run and a bulge-remodeling run, copy the
//  instantiated bulge conformations back to the original SWA poses, minimize once more,
//  and then output the pose with virtual residues. A fairly complex cleanup that
//  gives nice bulge conformations but with virtual residue types.
//
// Is this in use? Where? Can it move to its own file? -- rhiju, 2013
//
void
post_rebuild_bulge_assembly() ///Oct 22, 2011
{
  using namespace core::pose;
  using namespace core::chemical;
  using namespace core::kinematics;
  using namespace core::scoring;
	using namespace protocols::swa::rna;
	using namespace core::io::silent;
	using namespace core::conformation;
	using namespace ObjexxFCL;
	using namespace core::id;
	using namespace core::optimization;

	output_title_text( "Enter post_rebuild_bulge_assembly()", TR );

	ResidueTypeSetCAP rsd_set;
	rsd_set = core::chemical::ChemicalManager::get_instance()->residue_type_set( RNA );

	core::scoring::ScoreFunctionOP scorefxn = create_scorefxn();

	////////////////////////////////////////////////////////////////////////////////////

	if ( option[ start_silent ].user() == false ) utility_exit_with_message( "User need to pass in start_silent!" );
	if ( option[ start_tag ].user() == false ) utility_exit_with_message( "User need to pass in start_tag!" );
	if ( option[ in::file::silent ].user() == false ) utility_exit_with_message( "User need to pass in in::file::silent!" );
	if ( option[ in::file::tags ].user() == false ) utility_exit_with_message( "User need to pass in in::file::tags!" );
	if ( option[ out::file::silent ].user() == false ) utility_exit_with_message( "User need to pass in out::file::silent!" );

	bool const OUTPUT_PDB = option[ basic::options::OptionKeys::swa::rna::output_pdb ]();

	std::string const start_silent_file = option[start_silent ]();
	std::string const start_tag_name   =  option[start_tag ]();

	utility::vector1< std::string > const rebuild_silent_file_list = option[in::file::silent ]();
	utility::vector1< std::string > const rebuild_tag_name_list = option[in::file::tags]();

	std::string const rebuild_silent_file = rebuild_silent_file_list[1];
	std::string const rebuild_tag_name = rebuild_tag_name_list[1];

	std::string const out_silent_file = option[ out::file::silent ]();

	std::string const out_tag_name = start_tag_name;
	//std::string const out_tag_name= "R_" + start_tag_name.substr(2, start_tag_name.size()-2)

	///////////////////Create a 'mock' job_parameters only with the parameters called by output_data()////////////////////////
	StepWiseRNA_JobParametersOP	job_parameters = setup_simple_full_length_rna_job_parameters();
	StepWiseRNA_JobParametersCOP job_parameters_COP( job_parameters );
	Size const total_res = ( job_parameters->full_sequence() ).size();

  Pose start_pose;
  Pose rebuild_pose;

	import_pose_from_silent_file( start_pose,   start_silent_file,   start_tag_name );
	import_pose_from_silent_file( rebuild_pose, rebuild_silent_file, rebuild_tag_name );

	///I think that should able to extract the old_score line directly from the pose!
	runtime_assert( start_pose.total_residue() == total_res );
	runtime_assert( rebuild_pose.total_residue() == total_res );

	if ( OUTPUT_PDB ) dump_pdb( start_pose, "start_pose_" + start_tag_name + ".pdb" );
	if ( OUTPUT_PDB ) dump_pdb( rebuild_pose, "rebuild_pose_" + rebuild_tag_name + ".pdb" );

	/////////Dec 18, 2011: Deal with protonated adenosine. Actually code works fine without this..but still a good consistency test!/////////
	utility::vector1< core::Size > const protonated_H1_adenosine_list = job_parameters->protonated_H1_adenosine_list();

	for ( Size seq_num = 1; seq_num <= total_res; seq_num++ ){

		if ( has_virtual_rna_residue_variant_type( rebuild_pose, seq_num ) ) utility_exit_with_message( "rebuild_pose has virtaul_residue at seq_num( " + string_of( seq_num ) + " )!" );

		if ( protonated_H1_adenosine_list.has_value( seq_num ) ){

			runtime_assert( rebuild_pose.residue( seq_num ).has_variant_type( "PROTONATED_H1_ADENOSINE" ) );

			if ( has_virtual_rna_residue_variant_type( start_pose, seq_num ) ){

				if ( start_pose.residue( seq_num ).has_variant_type( "PROTONATED_H1_ADENOSINE" ) ){
					utility_exit_with_message( "seq_num( " + string_of( seq_num ) + " ) of start_pose has PROTONATED_H1_ADENOSINE variant type but is a virtual_residue!" );
				}

				//This ensures that the Adenosine base have the same atom_list in the start and rebuild pose!
				pose::remove_variant_type_from_pose_residue( rebuild_pose, "PROTONATED_H1_ADENOSINE", seq_num );
				std::cout << "removing PROTONATED_H1_ADENOSINE from seq_num " << seq_num << " of rebuild_pose since this seq_num is a virtual_residue in start_pose!" << std::endl;

			} else{

				if ( start_pose.residue( seq_num ).has_variant_type( "PROTONATED_H1_ADENOSINE" ) == false ){
					utility_exit_with_message( "seq_num( " + string_of( seq_num ) + " ) is in protonated_H1_adenosine_list but start_pose does not have PROTONATED_H1_ADENOSINE variant type!" );
				}

			}

		} else{

			if ( rebuild_pose.residue( seq_num ).has_variant_type( "PROTONATED_H1_ADENOSINE" ) ){
				utility_exit_with_message( "seq_num( " + string_of( seq_num ) + " ) is not in protonated_H1_adenosine_list but rebuild_pose has PROTONATED_H1_ADENOSINE variant type!" );
			}

		}

	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	core::pose::MiniPose const mini_rebuild_pose = *( core::pose::MiniPoseOP( new core::pose::MiniPose( rebuild_pose ) ) );
	if ( mini_rebuild_pose.total_residue() != total_res ){
		std::cout << "mini_rebuild_pose.total_residue() = " << mini_rebuild_pose.total_residue() << std::endl;
		std::cout << "total_res = " << total_res << std::endl;
		utility_exit_with_message( "mini_rebuild_pose.total_residue() != total_res" );
	}
	utility::vector1< utility::vector1< std::string > > const & chunk_atom_names_list = mini_rebuild_pose.atom_names_list();

	std::map< core::Size, core::Size > res_map;
	for ( Size n = 1; n <= start_pose.total_residue(); n++ ) {
		res_map[ n ] = n;
		std::cout << "chunk_seq_num = " << n << "| chunk_atom_names_list[chunk_seq_num].size() = " << chunk_atom_names_list[n].size() << std::endl;
	}


	/////Copy the conformation but nothing else. No energy and no cache data (having cache data can cause problem with column_name order in output silent_file!)//////
	/////OK...this should also copy the virtual_types and the fold_tree?//////////////////////////////////////////////////////////////////////////////////////////////
	//Pose output_pose=start_pose;
	ConformationOP copy_conformation = new Conformation();

	( *copy_conformation ) = start_pose.conformation();

	pose::Pose output_pose;
	output_pose.set_new_conformation( copy_conformation );

	if ( output_pose.total_residue() != total_res ) utility_exit_with_message( "output_pose.total_residue() != total_res" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//NEW_copy_dofs( output_pose, mini_rebuild_pose, res_map );
	copy_dofs_match_atom_names( output_pose, mini_rebuild_pose, res_map ); //Dec 28, 2011..STILL NEED TO VERIFY THAT THIS WORKS PROPERLY!
	//OK THE copy_dofs seem to correctly position every atom except for the OVU1, OVL1 and OVL2 at the chainbreak(s). This is becuase the rebuild_pose does not necessaringly have the same cutpoint position as the start_pose
	protocols::rna::assert_phosphate_nomenclature_matches_mini( output_pose ); //Just to be safe

	utility::vector1< Size > virtual_res_list;

	////////////////////////////////////////////////////////////////////////////////////
	for ( Size seq_num = 1; seq_num <= output_pose.total_residue(); seq_num++ ){
		///Oct 24, 2011 Should remove the virtual_variant type before minimizing to prevent artificial clashes!
		///Also currently have plan to not score torsional_potential if OVU1, OVL1 and OVL2 if corresponding atoms (O3', O5' and P1) are virtual (not implemented yet!)
		if ( has_virtual_rna_residue_variant_type( output_pose, seq_num ) ){
			virtual_res_list.push_back( seq_num );
			remove_virtual_rna_residue_variant_type( output_pose, seq_num );
		}
	}

	output_seq_num_list( "virtual_res_list = ", virtual_res_list, TR, 30 );
	////////////////////////////////////////////////////////////////////////////////////
	for ( Size seq_num = 1; seq_num <= output_pose.total_residue(); seq_num++ ){

		if ( output_pose.residue( seq_num ).has_variant_type( "CUTPOINT_LOWER" ) ){

			if ( seq_num == output_pose.total_residue() ) utility_exit_with_message( "seq_num == output_pose.total_residue() has CUTPOINT_LOWER variant_type!" );
			if ( !output_pose.fold_tree().is_cutpoint( seq_num ) ) utility_exit_with_message( "seq_num ( " + string_of( seq_num ) + " ) is not a cutpoint!" );
			runtime_assert ( output_pose.residue( seq_num + 1 ).has_variant_type( "CUTPOINT_UPPER" ) );

			////////////////////Copy the chain_break torsions///////////////////////////////////
			/////Important to copy torsion before calling rna_loop_closer///////////////////////
			/////Since copy_torsion with position OVU1, OVL1, OVL2 near the correct solution////
			/////This minimize changes in the other torsions (i.e. beta and gamma of 3'-res)////

			//alpha 3'-res aligns OVU1 of 3'-res to O3' of 5'-res
			//This doesn't seem to change the torsion? Possibly becuase OVU1 is already repositioned when COPY_DOF set the OP1 and OP2 pos?
			//output_pose.set_torsion( TorsionID( seq_num+1, id::BB,  1 ), rebuild_pose.residue(seq_num+1).mainchain_torsion(1)  );
			Real const output_alpha_torsion = output_pose.residue( seq_num + 1 ).mainchain_torsion( 1 );
			Real const rebuild_alpha_torsion = output_pose.residue( seq_num + 1 ).mainchain_torsion( 1 );

			Real const abs_diff = std::abs( output_alpha_torsion - rebuild_alpha_torsion );
			std::cout << "std::abs( output_alpha_torsion - rebuild_alpha_torsion ) = " << abs_diff << std::endl;
			if ( abs_diff > 0.000001 ) utility_exit_with_message( "std::abs( output_alpha_torsion - rebuild_alpha_torsion ) > 0.000001" );

			for ( Size n = 5; n <= 6; n++ ){
				//epsilon 5'-res aligns OVL1 of 5'-res ot P1  of 3'-res.
				//zeta of 5'-res aligns OVL2 of 5'-res to O5' of 3'-res
				output_pose.set_torsion( TorsionID( seq_num, id::BB,  n ), rebuild_pose.residue( seq_num ).mainchain_torsion( n ) );
			}

			////////////////////Copy the chain_break torsions////////////////////////////////////
			//This does slightly improve the CCD torsion (for TEST_MODE, no_minimize case!)
			//protocols::rna::RNA_LoopCloser rna_loop_closer;
			//rna_loop_closer.set_three_prime_alpha_only(true);
			//rna_loop_closer.apply( output_pose, seq_num );
			//////////////////////////////////////////////////////////////////////////////////////

			AtomTreeMinimizer minimizer;
	    float const dummy_tol( 0.00000025 );
	    bool const use_nblist( true );
	    MinimizerOptions options( "dfpmin", dummy_tol, use_nblist, false, false );
	    options.nblist_auto_update( true );

			core::kinematics::MoveMap mm;

			mm.set_bb( false );
			mm.set_chi( false );
			mm.set_jump( false );

			mm.set( TorsionID( seq_num , id::BB,  5 ), true );	//5'-res epsilon
			mm.set( TorsionID( seq_num , id::BB,  6 ), true );	//5'-res zeta
			mm.set( TorsionID( seq_num + 1, id::BB,  1 ), true );	//3'-res alpha

			minimizer.run( output_pose, mm, *( scorefxn ), options );
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	for ( Size seq_num = 1; seq_num <= output_pose.total_residue(); seq_num++ ){
		if ( virtual_res_list.has_value( seq_num ) ){
			apply_virtual_rna_residue_variant_type( output_pose, seq_num, true /*apply_check*/ );
		}
	}

	( *scorefxn )( output_pose );
	output_data( out_silent_file, out_tag_name, false /*write_score_only*/, output_pose, job_parameters->working_native_pose(),  job_parameters );
}


///////////////////////////////////////////////////////////////
void*
my_main( void* )
{

	using namespace protocols::swa::rna;
  using namespace basic::options;

	clock_t const my_main_time_start( clock() );

	std::string algorithm_input = option[ algorithm ];

	ensure_directory_for_out_silent_file_exists();

	if ( algorithm_input == "rna_resample_test" or algorithm_input == "rna_sample" ){
	  swa_rna_sample();
	} else if ( algorithm_input == "cluster_old" or algorithm_input == "rna_cluster" ){
		swa_rna_cluster();
	}	else if ( algorithm_input == "rna_sample_virtual_sugar" ){
	  rna_sample_virtual_sugar();
	} else if ( algorithm_input == "post_rebuild_bulge_assembly" ){
		post_rebuild_bulge_assembly();
	}	else if ( algorithm_input == "filter_combine_long_loop" ){
		filter_combine_long_loop();
	} else {
		utility_exit_with_message( "Invalid User - specified algorithm ( " + algorithm_input + " )!" );
	}

	protocols::viewer::clear_conformation_viewers();

	std::cout << "Total time took to run algorithm ( " << algorithm_input << " ): " << static_cast< Real > ( clock() - my_main_time_start ) / CLOCKS_PER_SEC << " seconds." << std::endl;

	// Do not change this last line -- its used in SWA runs.
	std::cout << "JOB_SUCCESSFULLY_COMPLETED" << std::endl;

  exit( 0 );

}


///////////////////////////////////////////////////////////////////////////////
int
main( int argc, char * argv [] )
{
	try {
  using namespace basic::options;

	utility::vector1< Size > blank_size_vector;
	utility::vector1< std::string > blank_string_vector;

	option.add_relevant( OptionKeys::swa::rna::fixed_res );

	//////////////General/////////////////////////////
	NEW_OPT( graphic, "Turn graphic on/off", true );
	NEW_OPT( Real_parameter_one, "free_variable for testing purposes ", 0.0 );
	NEW_OPT( distinguish_pucker, "distinguish pucker when cluster:both in sampler and clusterer", true );
	NEW_OPT( algorithm, "Specify algorithm to execute", "rna_sample" );

	//////////////Job_Parameters///////////
	NEW_OPT( sample_virtual_sugar_list, "optional: sample_virtual_sugar_list", blank_string_vector ); //July 20, 2011

	///////////////Sampler////////////
	NEW_OPT( sampler_native_rmsd_screen, "native_rmsd_screen ResidueSampler", false );
	NEW_OPT( sampler_native_screen_rmsd_cutoff, "sampler_native_screen_rmsd_cutoff", 2.0 );
	NEW_OPT( sampler_num_pose_kept, "optional: set_num_pose_kept by ResidueSampler", 108 );

	//////////////Minimizer////////////
	NEW_OPT( num_pose_minimize, "optional: set_num_pose_minimize by Minimizer", 999999 );

	//////////////Clusterer////////////
	NEW_OPT( clusterer_num_pose_kept, "optional: Num_pose_kept by the clusterer", 1000 );
	NEW_OPT( suite_cluster_radius, " individual_suite_cluster_radius ", 999.99 ); 							//IMPORTANT, DO NOT CHANGE DEFAULT VALUE!
	NEW_OPT( loop_cluster_radius, " loop_cluster_radius ", 999.99 ); 													//IMPORTANT, DO NOT CHANGE DEFAULT VALUE!
	NEW_OPT( clusterer_full_length_loop_rmsd_clustering, "use the full_length_rmsd function to calculate loop_rmsd", false ); //April 06, 2011: Should switch to true for all length_full clustering steps.

	//////////VDW_bin_screener//////////
	NEW_OPT( VDW_rep_screen_info, "VDW_rep_screen_info to create VDW_rep_screen_bin ( useful when building loop from large poses )", blank_string_vector ); //Jun 9, 2010
	NEW_OPT( VDW_rep_alignment_RMSD_CUTOFF, "use with VDW_rep_screen_info", 0.001 ); //Nov 12, 2010
	NEW_OPT( VDW_rep_delete_matching_res, "delete residues in VDW_rep_pose that exist in the working_pose", blank_string_vector ); //Feb 20, 2011
	NEW_OPT( VDW_rep_screen_physical_pose_clash_dist_cutoff, "The distance cutoff for VDW_rep_screen_with_physical_pose", 1.2 ); //March 23, 2011

	//////////////CombineLongLoopFilterer/////////////
	NEW_OPT( filter_for_previous_contact, "CombineLongLoopFilterer: filter_for_previous_contact", false ); //Sept 12, 2010
	NEW_OPT( filter_for_previous_clash, "CombineLongLoopFilterer: filter_for_previous_clash", false ); //Sept 12, 2010

	//////////////post_rebuild_bulge_assembly//////
	NEW_OPT( start_silent, "start_silent", "" ); //Oct 22, 2011
	NEW_OPT( start_tag, "start_tag", "" ); //Oct 22, 2011

	//////////////General/////////////////////////////
	///////The options below are for testing purposes. Please do not make any changes without first consulting/////////////
	///////Parin Sripakdeevong (sripakpa@stanford.edu) or Rhiju Das (rhiju@stanford.edu) //////////////////////////////////
	NEW_OPT( VERBOSE, "VERBOSE", false );
	NEW_OPT( parin_favorite_output, " parin_favorite_output ", true ); //Change to true on Oct 10, 2010
	NEW_OPT( integration_test, " integration_test ", false ); //March 16, 2012


	//////////////Job_Parameters///////////
	NEW_OPT( simple_full_length_job_params, "simple_full_length_job_params", false ); //Oct 31, 2011

	///////////////Sampler////////////
	NEW_OPT( sampler_cluster_rmsd, " Clustering rmsd of conformations in the sampler", 0.5 ); //DO NOT CHANGE THIS!
	NEW_OPT( skip_sampling, "no sampling step in rna_swa residue sampling", false );
	NEW_OPT( do_not_sample_multiple_virtual_sugar, " Samplerer: do_not_sample_multiple_virtual_sugar ", false );
	NEW_OPT( sample_ONLY_multiple_virtual_sugar, " Samplerer: sample_ONLY_multiple_virtual_sugar ", false );
	NEW_OPT( filterer_undercount_sugar_rotamers, "Undercount all sugar_rotamers as 1 count", false ); //July 29, 2011
	// FCC: Not doing anythin now... Just for consistency with swa_analytical_closure
	/////////
	NEW_OPT( sampler_extra_chi_rotamer, "Samplerer: extra_syn_chi_rotamer", false );
	NEW_OPT( sampler_extra_beta_rotamer, "Samplerer: extra_beta_rotamer", false );
	NEW_OPT( sampler_extra_epsilon_rotamer, "Samplerer: extra_epsilon_rotamer", true ); //Change this to true on April 9, 2011
	NEW_OPT( sample_both_sugar_base_rotamer, "Samplerer: Super hacky for SQUARE_RNA", false );
	NEW_OPT( reinitialize_CCD_torsions, "Samplerer: reinitialize_CCD_torsions: Reinitialize_CCD_torsion to zero before every CCD chain closure", false );
	NEW_OPT( PBP_clustering_at_chain_closure, "Samplerer: PBP_clustering_at_chain_closure", false );
	NEW_OPT( finer_sampling_at_chain_closure, "Samplerer: finer_sampling_at_chain_closure", false ); //Jun 9, 2010
	NEW_OPT( sampler_include_torsion_value_in_tag, "Samplerer:include_torsion_value_in_tag", true );
	NEW_OPT( sampler_allow_syn_pyrimidine, "sampler_allow_syn_pyrimidine", false ); //Nov 15, 2010
	NEW_OPT( sampler_assert_no_virt_sugar_sampling, "sampler_assert_no_virt_sugar_sampling", false ); //July 28, 2011
	NEW_OPT( sampler_try_sugar_instantiation, "for floating base sampling, try to instantiate sugar if it looks promising", false ); //July 28, 2011
	NEW_OPT( centroid_screen, "centroid_screen", true );
	NEW_OPT( allow_base_pair_only_centroid_screen, "allow_base_pair_only_centroid_screen", false ); //This only effect floating base sampling + dinucleotide.. deprecate option
	NEW_OPT( VDW_atr_rep_screen, "classic VDW_atr_rep_screen", true );
	NEW_OPT( sampler_perform_o2prime_pack, "perform O2' hydrogen packing inside StepWiseRNA_ResidueSampler", true );
	NEW_OPT( allow_bulge_at_chainbreak, "Allow sampler to replace chainbreak res with virtual_rna_variant if it looks have bad fa_atr score.", true );
	NEW_OPT( erraser, "Use KIC sampling", false );
	NEW_OPT( virtual_sugar_legacy_mode, "In virtual sugar sampling, use legacy protocol to match Parin's original workflow", false );

	//////////////Minimizer////////////
	NEW_OPT( minimize_and_score_native_pose, "minimize_and_score_native_pose ", false ); //Sept 15, 2010
	NEW_OPT( minimizer_perform_minimize, "minimizer_perform_minimize", true );
	NEW_OPT( minimizer_output_before_o2prime_pack, "minimizer_output_before_o2prime_pack", false );
	NEW_OPT( minimizer_perform_o2prime_pack, "perform O2' hydrogen packing inside StepWiseRNA_Minimizer", true ); //Jan 19, 2012
	NEW_OPT( minimizer_rename_tag, "Reorder and rename the tag by the energy_score", true ); //March 15, 2012
	NEW_OPT( native_edensity_score_cutoff, "native_edensity_score_cutoff", -1.0 ); //Fang's electron density code

	//////////////Clusterer ///////////////////////
	NEW_OPT( clusterer_two_stage_clustering, "Cluster is two stage..using triangle inequaility to speed up clustering", false ); //Change to false on April 9th 2011
	NEW_OPT( clusterer_keep_pose_in_memory, "reduce memory usage for the clusterer", true ); //Aug 6, 2010
	NEW_OPT( clusterer_quick_alignment, "quick alignment during clusterer...only work if the alignment residues are fixed ", false );
	NEW_OPT( clusterer_align_only_over_base_atoms, "align_only_over_base_atoms in clusterer alignment ", true ); //Add option in Aug 20, 2011
	NEW_OPT( clusterer_optimize_memory_usage, "clusterer_optimize_memory_usage ", false );
	NEW_OPT( score_diff_cut, "score difference cut for clustering", 1000000.0 );							//IMPORTANT, DO NOT CHANGE DEFAULT VALUE!
	NEW_OPT( clusterer_perform_score_diff_cut, "score difference cut for clustering", false ); //IMPORTANT, LEAVE THE DEFAULT AS FALSE!

	//////////////Clusterer Post-Analysis////////////
	NEW_OPT( recreate_silent_struct, "Special mode to recreate_silent_struct for clusterer output...for analysis purposes", false );
	NEW_OPT( clusterer_write_score_only, "clusterer_write_score_only/ only effect recreate_silent_struct mode  ", false );
	NEW_OPT( clusterer_perform_filters, "Other filters such as for specific puckers and chi conformations", false ); //June 13, 2011
	NEW_OPT( clusterer_perform_VDW_rep_screen, "filter for VDW clash with the VDW_rep_pose in clusterer ", false ); //March 20, 2011
	NEW_OPT( clusterer_use_best_neighboring_shift_RMSD, "Special mode for clusterer output...for analysis purposes", false ); //Dec 10, 2011
	NEW_OPT( clusterer_ignore_FARFAR_no_auto_bulge_tag, "clusterer_ignore_FARFAR_no_auto_bulge_tag", false ); //Sept 06, 2011
	NEW_OPT( clusterer_ignore_FARFAR_no_auto_bulge_parent_tag, "clusterer_ignore_FARFAR_no_auto_bulge_parent_tag", false ); //Sept 06, 2011
	NEW_OPT( clusterer_ignore_unmatched_virtual_res, "clusterer_ignore_unmatched_virtual_res", false ); //Sept 06, 2011
	NEW_OPT( clusterer_min_num_south_sugar_filter, "clusterer_min_num_south_sugar_filter", 0 ); //Oct 02, 2011
	NEW_OPT( add_lead_zero_to_tag, "Add lead zero to clusterer output tag ", false );
	NEW_OPT( skip_clustering, "keep every pose, no clustering", false );
	NEW_OPT( clusterer_rename_tags, "clusterer_rename_tags", true );
	NEW_OPT( whole_struct_cluster_radius, " whole_struct_cluster_radius ", 0.5 ); //IMPORTANT DO NOT CHANGE
	NEW_OPT( constraint_chi, "Constrain the chi angles", false );
	NEW_OPT( rm_virt_phosphate, "Remove virtual phosphate patches during minimization", false );

  ////////////////////////////////////////////////////////////////////////////
  // setup
  ////////////////////////////////////////////////////////////////////////////
  core::init::init( argc, argv );


  ////////////////////////////////////////////////////////////////////////////
  // end of setup
  ////////////////////////////////////////////////////////////////////////////

  protocols::viewer::viewer_main( my_main );
	} catch ( utility::excn::EXCN_Base const & e ) {
		std::cout << "caught exception " << e.msg() << std::endl;
	}
}



