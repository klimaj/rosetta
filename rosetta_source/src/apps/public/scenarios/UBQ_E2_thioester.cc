// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   src/apps/public/scenarios/UBQ_E2_thioester.cc
/// @brief  this application is a one-shot for modeling the thioester bond between UBQ and an E2
/// @author Steven Lewis

// Unit Headers

// Project Headers
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/pose/PDBInfo.hh>
#include <core/pose/PDBPoseMap.hh>

#include <core/import_pose/import_pose.hh>

#include <core/conformation/Residue.hh>
#include <core/conformation/Conformation.hh>

#include <core/pack/task/TaskFactory.hh>
// AUTO-REMOVED #include <core/pack/task/PackerTask.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <protocols/toolbox/task_operations/RestrictByCalculatorsOperation.hh>

#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

#include <core/scoring/constraints/AtomPairConstraint.hh>
#include <core/scoring/constraints/BoundConstraint.hh>
#include <core/scoring/constraints/AmbiguousConstraint.hh>

#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
// AUTO-REMOVED #include <core/kinematics/util.hh>

#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/ResidueType.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/chemical/ChemicalManager.hh>

#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/Job.hh>

#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
// AUTO-REMOVED #include <protocols/loops/loops_main.hh>

//movers
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/simple_moves/BackboneMover.hh>
#include <protocols/simple_moves/MinMover.hh>
#include <protocols/moves/MoverContainer.hh> //Sequence Mover
#include <protocols/simple_moves/RotamerTrialsMover.hh>
#include <protocols/simple_moves/TaskAwareMinMover.hh>
// AUTO-REMOVED #include <protocols/moves/OutputMovers.hh> //pdbdumpmover
#include <protocols/simple_moves/TorsionDOFMover.hh>
#include <protocols/moves/JumpOutMover.hh>
#include <protocols/loops/loop_closure/kinematic_closure/KinematicMover.hh>
#include <protocols/loops/loop_closure/kinematic_closure/KinematicWrapper.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>

#include <basic/MetricValue.hh>
#include <core/pose/metrics/CalculatorFactory.hh>
#include <core/pose/metrics/simple_calculators/InterfaceSasaDefinitionCalculator.hh>
#include <core/pose/metrics/simple_calculators/InterfaceNeighborDefinitionCalculator.hh>
#include <protocols/toolbox/pose_metric_calculators/NeighborhoodByDistanceCalculator.hh>
#include <protocols/toolbox/pose_metric_calculators/NeighborsByDistanceCalculator.hh>
#include <protocols/analysis/InterfaceAnalyzerMover.hh>

// Numeric Headers
#include <numeric/conversions.hh>
// AUTO-REMOVED #include <numeric/xyz.io.hh>

// Utility Headers
#include <devel/init.hh>
#include <basic/options/option.hh>
#include <basic/Tracer.hh>
#include <utility/vector1.hh>
#include <utility/exit.hh>
#include <basic/prof.hh>

// C++ headers
#include <string>

// option key includes
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/AnchoredDesign.OptionKeys.gen.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>
#include <basic/options/keys/loops.OptionKeys.gen.hh>

#include <utility/vector0.hh>

//Auto Headers
#include <core/kinematics/AtomTree.hh>

//local options
basic::options::FileOptionKey const UBQpdb("UBQpdb");
basic::options::FileOptionKey const E2pdb("E2pdb");
basic::options::IntegerOptionKey const E2_residue("E2_residue");
basic::options::RealOptionKey const SASAfilter("SASAfilter");
basic::options::RealOptionKey const scorefilter("scorefilter");
basic::options::RealOptionKey const constraintweight("constraintweight");

//tracers
using basic::Error;
using basic::Warning;
static basic::Tracer TR("apps.public.scenarios.UBQ_E2_thioester");

class UBQ_E2Mover : public protocols::moves::Mover {
public:
	UBQ_E2Mover()
	: init_for_input_yet_(false),
		fullatom_scorefunction_(NULL),
		task_factory_(NULL),
		thioester_mm_(NULL),
		loop_(), //we want default ctor
		atomIDs(8, core::id::BOGUS_ATOM_ID ),
		InterfaceSasaDefinition_("InterfaceSasaDefinition_" + 1),
		IAM_(new protocols::analysis::InterfaceAnalyzerMover)
	{
		//set up fullatom scorefunction
		using namespace core::scoring;
		fullatom_scorefunction_ = getScoreFunction();
		fullatom_scorefunction_->set_weight( atom_pair_constraint, basic::options::option[constraintweight].value());

		TR << "Using fullatom scorefunction from commandline:\n" << *fullatom_scorefunction_;

		using namespace core::pose::metrics;
		using namespace protocols::toolbox::pose_metric_calculators;
		//magic number: chains 1 and 2; set up interface SASA calculator
		if( !CalculatorFactory::Instance().check_calculator_exists( InterfaceSasaDefinition_ ) ){
			CalculatorFactory::Instance().register_calculator( InterfaceSasaDefinition_, new core::pose::metrics::simple_calculators::InterfaceSasaDefinitionCalculator(core::Size(1), core::Size(2)));
		}

		IAM_->set_use_centroid_dG(false);
	}

	///@brief init_on_new_input system allows for initializing these details the first time apply() is called.  the job distributor will reinitialize the whole mover when the input changes (a freshly constructed mover, which will re-run this on first apply().
	virtual
	void
	init_on_new_input() {
		init_for_input_yet_ = true;


		///////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////Create starting complex pose/////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////

		TR << "Creating starting pose..." << std::endl;

		//read poses
		core::pose::Pose E2;
		core::import_pose::pose_from_pdb( E2, basic::options::option[E2pdb].value() );
		core::Size const E2length = E2.total_residue();

		core::pose::Pose UBQ;
		core::import_pose::pose_from_pdb( UBQ, basic::options::option[UBQpdb].value() );
		core::Size const UBQlength = UBQ.total_residue();

		//determine cysteine target
		runtime_assert(E2.conformation().num_chains() == 1);
		char const E2chain(E2.pdb_info()->chain(1));
		core::Size const E2_cys(E2.pdb_info()->pdb2pose(E2chain, basic::options::option[E2_residue].value()));
		runtime_assert(E2.residue_type(E2_cys).aa() == core::chemical::aa_cys);

		//determine c_term target on UBQ
		core::Size const UBQ_term = UBQlength;

		//strip C-term from UBQ - best to do this with a full replace to re-draw the carboxyl oxygen
		//UBQ.dump_pdb("pre-removeUQB.pdb");
		core::chemical::ResidueTypeSetCAP fa_standard(core::chemical::ChemicalManager::get_instance()->residue_type_set(core::chemical::FA_STANDARD));
		UBQ.conformation().delete_residue_slow( UBQ_term );
		UBQ.append_residue_by_bond( *(core::conformation::ResidueFactory::create_residue(fa_standard->name_map("GLY")) ) );
		//UBQ.dump_pdb("post-removeUQB.pdb");

		//replace cysteine
		core::chemical::ResidueType const & cyx_rsd_type( fa_standard->name_map("CYX") );
		//E2.dump_pdb("prereplace_E2.pdb");
		core::pose::replace_pose_residue_copying_existing_coordinates( E2, E2_cys, cyx_rsd_type );
		//E2.dump_pdb("postreplace_E2.pdb");


		// check safety of connections (from phil)
		core::chemical::ResidueType const & ubq_rsd_type( UBQ.residue_type( UBQ_term ) );
		core::Size const cyx_connid( 3 );
		core::Size const ubq_connid( 2 );

		runtime_assert( cyx_rsd_type.n_residue_connections() == cyx_connid &&
			cyx_rsd_type.lower_connect_id() != cyx_connid &&
			cyx_rsd_type.upper_connect_id() != cyx_connid );

		runtime_assert( ubq_rsd_type.n_residue_connections() == ubq_connid &&
			ubq_rsd_type.lower_connect_id() != ubq_connid);

		//E2.conformation().show_residue_connections();
		//UBQ.conformation().show_residue_connections();

		//cross fingers - making the actual connection!
		/*void
			append_residue_by_bond(
			conformation::Residue const & new_rsd,
			bool const build_ideal_geometry = false,
			int const connection = 0,
			Size const anchor_residue = 0,
			int const anchor_connection = 0,
			bool const start_new_chain = false
			)*/
		core::pose::Pose complex(E2);
		complex.append_residue_by_bond( UBQ.residue( UBQ_term ), true, ubq_connid, E2_cys, cyx_connid );
		//complex.dump_pdb("just1_complex.pdb");

		//not that this does anything
		complex.conformation().insert_ideal_geometry_at_residue_connection( E2_cys, cyx_connid );

		core::Size const ubq_pos( complex.total_residue() );
		core::id::AtomID const atom0( cyx_rsd_type.atom_index( "C" ), E2_cys );
		core::id::AtomID const atom1( cyx_rsd_type.atom_index( "CA" ), E2_cys );
		core::id::AtomID const atom2( cyx_rsd_type.atom_index( "CB" ), E2_cys );
		core::id::AtomID const atom3( cyx_rsd_type.atom_index( "SG" ), E2_cys );
		core::id::AtomID const atom4( ubq_rsd_type.atom_index( "C"  ), ubq_pos );
		core::id::AtomID const atom5( ubq_rsd_type.atom_index( "CA" ), ubq_pos );
		core::id::AtomID const atom6( ubq_rsd_type.atom_index( "N"  ), ubq_pos );

		//starting values derived from 1FXT.pdb
		complex.conformation().set_torsion_angle( atom0, atom1, atom2, atom3, numeric::conversions::radians(106.5) );
		complex.conformation().set_torsion_angle( atom1, atom2, atom3, atom4, numeric::conversions::radians(-60.0) );
		complex.conformation().set_torsion_angle( atom2, atom3, atom4, atom5, numeric::conversions::radians(180.0) );
		complex.conformation().set_torsion_angle( atom3, atom4, atom5, atom6, numeric::conversions::radians(100.5) );
		//complex.dump_pdb("just1_complex2.pdb");

		//now add the rest of ubiquitin
		for( core::Size i=UBQ_term-1; i>= 1; --i ) {
			complex.prepend_polymer_residue_before_seqpos( UBQ.residue(i), E2length+1, false );
		}

		core::Size const complexlength( complex.total_residue());
		complex.conformation().insert_ideal_geometry_at_polymer_bond( complexlength-1 );
		complex.conformation().insert_chain_ending(E2length);
		//complex.dump_pdb("initcomplex.pdb");

		//pack atom ID vector
		atomIDs[1] = atom0;
		atomIDs[2] = atom1;
		atomIDs[3] = atom2;
		atomIDs[4] = atom3;
		atomIDs[5] = core::id::AtomID( ubq_rsd_type.atom_index("C" ), complexlength );
		atomIDs[6] = core::id::AtomID( ubq_rsd_type.atom_index("CA" ), complexlength );
		atomIDs[7] = core::id::AtomID( ubq_rsd_type.atom_index("N" ), complexlength );
		atomIDs[8] = core::id::AtomID( ubq_rsd_type.atom_index("C" ), complexlength-1 );

		starting_pose_ = complex;
		starting_pose_.dump_pdb("starting_complex.pdb");
		TR << "starting pose finished." << std::endl;

		///////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////Finish starting complex pose/////////////////////////////////////////////
		//////////////////////Start creating move accessory data///////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////

		//setup MoveMaps
		//small/shear behave improperly @ the last residue - psi is considered nonexistent and the wrong phis apply.
		thioester_mm_ = new core::kinematics::MoveMap;
		//thioester_mm_->set_bb(complexlength, true);
		//thioester_mm_->set(core::id::TorsionID(complexlength, core::id::BB, core::id::phi_torsion), true);
		//thioester_mm_->set(core::id::TorsionID(complexlength, core::id::BB, core::id::psi_torsion), true);
		thioester_mm_->set_bb(complexlength-1, true);
		thioester_mm_->set_bb(complexlength-2, true);
		//thioester_mm_->set(complex.atom_tree().torsion_angle_dof_id(atomIDs[2], atomIDs[3], atomIDs[4], atomIDs[5]), false);

		//setup loop
		std::set< core::Size > loop_posns;
		if ( basic::options::option[ basic::options::OptionKeys::loops::loop_file ].user() ) {
			loop_ = *(protocols::loops::get_loops_from_file().begin());
			TR << "loop " <<  loop_ << std::endl;
			//set up interface-plus-neighbors-positions operation
			for (core::Size j(loop_.start()), end(loop_.stop()); j <= end; ++j){
				loop_posns.insert(j);
			}//for each residue in loop
		} //no else needed - default loop is safe enough

		//setup of TaskFactory
		using namespace core::pack::task;
		using namespace core::pack::task::operation;
		task_factory_ = new TaskFactory;
		task_factory_->push_back( new InitializeFromCommandline );
		if ( basic::options::option[ basic::options::OptionKeys::packing::resfile ].user() ) {
			task_factory_->push_back( new ReadResfile );
		}
		//task_factory_->push_back( new protocols::toolbox::task_operations::RestrictToInterfaceOperation );
		task_factory_->push_back( new IncludeCurrent );
		//prevent repacking at linkage cysteine!
		PreventRepackingOP prevent(new PreventRepacking);
		prevent->include_residue(E2_cys);
		task_factory_->push_back(prevent);

		std::string const interface_calc("UBQE2_InterfaceNeighborDefinitionCalculator");
		std::string const neighborhood_calc("UBQE2_NeighborhoodByDistanceCalculator");
		core::pose::metrics::CalculatorFactory::Instance().register_calculator( interface_calc, new core::pose::metrics::simple_calculators::InterfaceNeighborDefinitionCalculator( core::Size(1), core::Size(2)) );
		core::pose::metrics::CalculatorFactory::Instance().register_calculator( neighborhood_calc, new protocols::toolbox::pose_metric_calculators::NeighborhoodByDistanceCalculator( loop_posns ) );

		//this is the constructor parameter for the calculator - pairs of calculators and calculations to perform
		utility::vector1< std::pair< std::string, std::string> > calcs_and_calcns;
		calcs_and_calcns.push_back(std::make_pair(interface_calc, "interface_residues"));
		calcs_and_calcns.push_back(std::make_pair(neighborhood_calc, "neighbors"));

		using protocols::toolbox::task_operations::RestrictByCalculatorsOperation;
		task_factory_->push_back(new RestrictByCalculatorsOperation( calcs_and_calcns ));

		//create constraints
// 		CA-CA distance for charge pair in 1fxt: 10 angstroms (E117, R42), becomes (E109, R??); absolute resids 106, 196)
// I44 CA-CA distance to three nearby residues: 6.2 (1fxt A111, I44) (absolute resids 102, 198)
//     7.7	  1fxt Q114, I44; absolute resids 105, 198
//     7.9 ?? comes to same E as in charge pair
// basin of at least 2 angstroms zero constraint?

		core::pose::metrics::CalculatorFactory::Instance().register_calculator( "I44neighbors", new protocols::toolbox::pose_metric_calculators::NeighborsByDistanceCalculator( 198 ) );

		using namespace core::scoring::constraints;
		using core::id::AtomID;
		AmbiguousConstraintOP ambig_ionpair( new AmbiguousConstraint() );
		BoundFuncOP chargepair( new BoundFunc( 0, 2.5, 2, "chargepair") );
		ambig_ionpair->add_individual_constraint(new AtomPairConstraint( AtomID(8, 106), AtomID(10, 196), chargepair ) );
		ambig_ionpair->add_individual_constraint(new AtomPairConstraint( AtomID(8, 106), AtomID(11, 196), chargepair ) );
		ambig_ionpair->add_individual_constraint(new AtomPairConstraint( AtomID(9, 106), AtomID(10, 196), chargepair ) );
		ambig_ionpair->add_individual_constraint(new AtomPairConstraint( AtomID(9, 106), AtomID(11, 196), chargepair ) );
		starting_pose_.add_constraint( ambig_ionpair );

		AmbiguousConstraintOP ambig_helixcap( new AmbiguousConstraint() );
		BoundFuncOP helixcap( new BoundFunc( 0, 2, 5, "helixcap") );
		ambig_helixcap->add_individual_constraint(new AtomPairConstraint( AtomID(4, 102), AtomID(13, 196), helixcap ) );
		ambig_helixcap->add_individual_constraint(new AtomPairConstraint( AtomID(4, 102), AtomID(15, 196), helixcap ) );
		starting_pose_.add_constraint( ambig_helixcap );

		BoundFuncOP I44_gen( new BoundFunc( 0, 7, 5, "I44_generic") );

		AmbiguousConstraintOP ambig( new AmbiguousConstraint() );

		//yes, this is a raw array
		core::Size e2_face_array[] = { 4, 8, 11, 15, 16, 17, 18, 19, 21, 38, 39, 40, 41, 42, 43, 44, 46, 47, 60, 61, 62, 63, 64, 66, 77, 79, 82, 83, 85, 87, 88, 89, 90, 91, 92, 94, 95, 96, 98, 99, 101, 102, 103, 105, 108, 109, 110, 111, 113, 115, 117, 121, 122, 124, 125, 128, 134, 135};

		utility::vector1< core::Size > e2_face_vec(e2_face_array, e2_face_array+(sizeof(e2_face_array)/sizeof(e2_face_array[0])));

		for(core::Size i(1); i<=e2_face_vec.size(); ++i){
			ambig->add_individual_constraint( new AtomPairConstraint( AtomID(2, e2_face_vec[i]), AtomID(2, 198), I44_gen ) );
			TR << e2_face_vec[i] << " ";
		}
		TR << "size " << e2_face_vec.size() << std::endl;

		starting_pose_.add_constraint( ambig );

		//starting_pose_.add_constraint( new AtomPairConstraint( AtomID(2, 102), AtomID(2, 198), new BoundFunc( 0, 9, 10, "I44_1") ) );
		//starting_pose_.add_constraint( new AtomPairConstraint( AtomID(2, 105), AtomID(2, 198), new BoundFunc( 0, 10, 10, "I44_2") ) );

// 		TR << " " << starting_pose_.xyz(AtomID(2, 106))<< " " <<  starting_pose_.xyz(AtomID(2, 196))
// 	<< " " <<		starting_pose_.xyz(AtomID(2, 102))<< " " << starting_pose_.xyz(AtomID(2, 198))
// 	<< " " <<		starting_pose_.xyz(AtomID(2, 105))<< " " <<starting_pose_.xyz( AtomID(2, 198)) << std::endl;
	}
	virtual ~UBQ_E2Mover(){};

	virtual
	void
	apply( core::pose::Pose & pose ){
		if( !init_for_input_yet_ ) init_on_new_input();

		pose = starting_pose_;

		//TR << "foldtree, movemap: " << std::endl;
		//core::kinematics::simple_visualize_fold_tree_and_movemap( pose.fold_tree(), *movemap_, TR);

		/////////////////fullatom Monte Carlo//////////////////////////////////////////////////////////
		//make the monte carlo object
		using protocols::moves::MonteCarlo;
		using protocols::moves::MonteCarloOP;
		using basic::options::option;
		using namespace basic::options::OptionKeys::AnchoredDesign;
		MonteCarloOP mc( new MonteCarlo( pose, *fullatom_scorefunction_, option[ refine_temp ].value() ) );

		//////////////////////////Small/ShearMovers////////////////////////////////////////////////////////
		protocols::simple_moves::BackboneMoverOP small_mover = new protocols::simple_moves::SmallMover(thioester_mm_, 0.8, 1);
		small_mover->angle_max( 'H', 4.0 );
		small_mover->angle_max( 'E', 4.0 );
		small_mover->angle_max( 'L', 4.0 );

		protocols::simple_moves::BackboneMoverOP shear_mover = new protocols::simple_moves::ShearMover(thioester_mm_, 0.8, 1);
		shear_mover->angle_max( 'H', 4.0 );
		shear_mover->angle_max( 'E', 4.0 );
		shear_mover->angle_max( 'L', 4.0 );

		protocols::simple_moves::TorsionDOFMoverOP DOF_mover_chi1(new protocols::simple_moves::TorsionDOFMover);
		DOF_mover_chi1->set_DOF(atomIDs[1], atomIDs[2], atomIDs[3], atomIDs[4]);
		DOF_mover_chi1->check_mmt(true);
		DOF_mover_chi1->temp(0.4);
		DOF_mover_chi1->set_angle_range(-180, 180);
		DOF_mover_chi1->tries(1000);

		protocols::simple_moves::TorsionDOFMoverOP DOF_mover_chi2(new protocols::simple_moves::TorsionDOFMover);
		DOF_mover_chi2->set_DOF(atomIDs[2], atomIDs[3], atomIDs[4], atomIDs[5]);
		DOF_mover_chi2->check_mmt(true);
		DOF_mover_chi2->temp(0.4);
		DOF_mover_chi2->set_angle_range(-180, 180);
		DOF_mover_chi2->tries(1000);

		protocols::simple_moves::TorsionDOFMoverOP DOF_mover_thioester(new protocols::simple_moves::TorsionDOFMover);
		DOF_mover_thioester->set_DOF(atomIDs[3], atomIDs[4], atomIDs[5], atomIDs[6]);
		DOF_mover_thioester->check_mmt(true);
		DOF_mover_thioester->temp(0.4);
		DOF_mover_thioester->set_angle_range(-180, 180);
		DOF_mover_thioester->tries(1000);

		protocols::simple_moves::TorsionDOFMoverOP DOF_mover_psi(new protocols::simple_moves::TorsionDOFMover);
		DOF_mover_psi->set_DOF(atomIDs[4], atomIDs[5], atomIDs[6], atomIDs[7]);
		DOF_mover_psi->check_mmt(true);
		DOF_mover_psi->temp(0.4);
		DOF_mover_psi->set_angle_range(-180, 180);
		DOF_mover_psi->tries(1000);

		protocols::simple_moves::TorsionDOFMoverOP DOF_mover_phi(new protocols::simple_moves::TorsionDOFMover);
		DOF_mover_phi->set_DOF(atomIDs[5], atomIDs[6], atomIDs[7], atomIDs[8]);
		DOF_mover_phi->check_mmt(true);
		DOF_mover_phi->temp(0.4);
		DOF_mover_phi->set_angle_range(-180, 180);
		DOF_mover_phi->tries(1000);

		protocols::moves::RandomMoverOP backbone_mover( new protocols::moves::RandomMover() );
		backbone_mover->add_mover(small_mover, 2.0);
		backbone_mover->add_mover(shear_mover, 1.0);
		backbone_mover->add_mover(DOF_mover_chi1, 0.75);
		backbone_mover->add_mover(DOF_mover_chi2, 0.75);
		backbone_mover->add_mover(DOF_mover_thioester, 0.75);
		backbone_mover->add_mover(DOF_mover_psi, 0.75);
		backbone_mover->add_mover(DOF_mover_phi, 0.75);

		///////////////////////////loop movement/////////////////////////////////////////////////////
		if( loop_.stop() - loop_.start() >= 3 ) { //empty loop; skip it!
			//make kinematic mover
			using protocols::loops::loop_closure::kinematic_closure::KinematicMoverOP;
			using protocols::loops::loop_closure::kinematic_closure::KinematicMover;
			KinematicMoverOP kin_mover( new KinematicMover() );
			kin_mover->set_temperature( 0.8 );
			kin_mover->set_vary_bondangles( true );
			kin_mover->set_sample_nonpivot_torsions( true );
			kin_mover->set_rama_check( true );

			using protocols::loops::loop_closure::kinematic_closure::KinematicWrapperOP;
			using protocols::loops::loop_closure::kinematic_closure::KinematicWrapper;
			KinematicWrapperOP kin_wrapper( new KinematicWrapper(kin_mover, loop_));

			backbone_mover->add_mover(kin_wrapper, 5);

		}

		/////////////////////////minimize backbone DOFs//////////////////////////////////////////////
		using protocols::simple_moves::MinMoverOP;
		using protocols::simple_moves::MinMover;
		protocols::simple_moves::MinMoverOP min_mover = new protocols::simple_moves::MinMover(
																				thioester_mm_,
																				fullatom_scorefunction_,
																				basic::options::option[ basic::options::OptionKeys::run::min_type ].value(),
																				0.01,
																				true /*use_nblist*/ );

		/////////////////////////////////rotamer trials mover///////////////////////////////////////////
		using protocols::simple_moves::RotamerTrialsMoverOP;
		using protocols::simple_moves::EnergyCutRotamerTrialsMover;
		protocols::simple_moves::RotamerTrialsMoverOP rt_mover(new protocols::simple_moves::EnergyCutRotamerTrialsMover(
																																	fullatom_scorefunction_,
																																	task_factory_,
																																	mc,
																																	0.01 /*energycut*/ ) );

		///////////////////////package RT/min for JumpOutMover////////////////////////////////////////
		protocols::moves::SequenceMoverOP RT_min_seq( new protocols::moves::SequenceMover );
		RT_min_seq->add_mover(rt_mover);
		RT_min_seq->add_mover(min_mover);

		protocols::moves::JumpOutMoverOP bb_if_RT_min( new protocols::moves::JumpOutMover(
																																											backbone_mover,
																																											RT_min_seq,
																																											fullatom_scorefunction_,
																																											20.0));

		///////////////////////////////repack///////////////////////////////////////////////
		protocols::simple_moves::PackRotamersMoverOP pack_mover = new protocols::simple_moves::PackRotamersMover;
		pack_mover->task_factory( task_factory_ );
		pack_mover->score_function( fullatom_scorefunction_ );

		protocols::simple_moves::MinMoverOP min_mover_pack = new protocols::simple_moves::MinMover(
																						 thioester_mm_,
																						 fullatom_scorefunction_,
																						 basic::options::option[ basic::options::OptionKeys::run::min_type ].value(),
																						 0.01,
																						 true /*use_nblist*/ );

		using protocols::simple_moves::TaskAwareMinMoverOP;
		using protocols::simple_moves::TaskAwareMinMover;
		protocols::simple_moves::TaskAwareMinMoverOP TAmin_mover = new protocols::simple_moves::TaskAwareMinMover(min_mover_pack, task_factory_);

		/////////////////////////////////////////refine loop///////////////////////////////////////////

		core::Size const refine_applies = option[ refine_cycles ].value(); //default 5
		core::Size const repack_cycles = option[ refine_repack_cycles ].value();
		//core::Size const min_cycles = repack_cycles/2;
		TR << "   Current     Low    total cycles =" << refine_applies << std::endl;
		for ( core::Size i(1); i <= refine_applies; ++i ) {
			//pdb_out1.apply(pose);
			if( (i % repack_cycles == 0) || (i == refine_applies) ) { //full repack
				pack_mover->apply(pose);
				TAmin_mover->apply(pose);
				//} else if ( i % min_cycles == 0 ) { //minimize
				//min_mover->apply(pose);
			} else {
				bb_if_RT_min->apply(pose);
			}

			mc->boltzmann(pose);
			TR << i << "  " << mc->last_accepted_score() << "  " << mc->lowest_score() << std::endl;
		}//end the exciting for loop
		mc->recover_low( pose );

		//filter on SASAs of 1000?
		(*fullatom_scorefunction_)(pose);
		analyze_and_filter(pose);
		return;
	}

	void
	analyze_and_filter(core::pose::Pose & pose){

		//Filter on total score
		core::Real const score((*fullatom_scorefunction_)(pose));
		if( score > basic::options::option[scorefilter].value() ){
			set_last_move_status(protocols::moves::FAIL_RETRY);
			TR << "total score filter failed; score " << score << std::endl;
			return;
		}

		//filter on interface SASA - requires some hacking to break up thioester
		core::pose::Pose copy(pose);
		//hack the pose up for analysis purposes
		core::Size const cbreak(copy.conformation().chain_end(1));
		using core::kinematics::Edge;
		core::kinematics::FoldTree main_tree(copy.total_residue());
		main_tree.clear();
		main_tree.add_edge(Edge(1, cbreak, Edge::PEPTIDE));
		main_tree.add_edge(Edge(cbreak+1, copy.total_residue(), Edge::PEPTIDE));
		main_tree.add_edge(Edge(cbreak, cbreak+1, 1));
		main_tree.reorder(1);
		//TR << main_tree << std::endl;
		copy.fold_tree(main_tree);

 		//Filter on SASA
		basic::MetricValue< core::Real > mv_delta_sasa;
		copy.metric(InterfaceSasaDefinition_, "delta_sasa", mv_delta_sasa);
		if(mv_delta_sasa.value() < basic::options::option[SASAfilter].value()){
			set_last_move_status(protocols::moves::FAIL_RETRY);
			TR << "interface SASA filter failed; SASA " << mv_delta_sasa.value() << std::endl;
			return;
		}

		//passed filters; run IAM
		IAM_->apply(copy);

		//print mobile region fine-grained data
		protocols::jd2::JobOP job_me(protocols::jd2::JobDistributor::get_instance()->current_job());
		using numeric::conversions::degrees;
		job_me->add_string_real_pair("cysteine_chi1_C-CA-CB-SG", degrees(pose.atom_tree().torsion_angle(atomIDs[1], atomIDs[2], atomIDs[3], atomIDs[4])));
		job_me->add_string_real_pair("cysteine_chi2_CA-CB-SG-C", degrees(pose.atom_tree().torsion_angle(atomIDs[2], atomIDs[3], atomIDs[4], atomIDs[5])));
		job_me->add_string_real_pair("thioester_CB-SG-C-CA", degrees(pose.atom_tree().torsion_angle(atomIDs[3], atomIDs[4], atomIDs[5], atomIDs[6])));
		job_me->add_string_real_pair("glycine_psi_SG-C-CA-N", degrees(pose.atom_tree().torsion_angle(atomIDs[4], atomIDs[5], atomIDs[6], atomIDs[7])));
		job_me->add_string_real_pair("glycine_phi_C-CA-N-C", degrees(pose.atom_tree().torsion_angle(atomIDs[5], atomIDs[6], atomIDs[7], atomIDs[8])));

		//I44 neighbors
		basic::MetricValue< core::Size > I44numn;
		copy.metric("I44neighbors", "num_neighbors", I44numn);
		job_me->add_string_real_pair("I44neighbors", I44numn.value());

		set_last_move_status(protocols::moves::MS_SUCCESS);
		return;
	}

	virtual
	protocols::moves::MoverOP
	fresh_instance() const {
		return new UBQ_E2Mover;
	}

	virtual
	bool
	reinitialize_for_each_job() const { return false; }

	virtual
	bool
	reinitialize_for_new_input() const { return false; }

	virtual
	std::string
	get_name() const { return "UBQ_E2Mover"; }

private:
	bool init_for_input_yet_;

	core::scoring::ScoreFunctionOP fullatom_scorefunction_;
	core::pack::task::TaskFactoryOP task_factory_;
	core::kinematics::MoveMapOP thioester_mm_;
// 	core::kinematics::MoveMapOP loop_mm_;
// 	core::kinematics::MoveMapOP all_mm_;

	protocols::loops::Loop loop_;

	///@brief vector contains atomIDs for thioester bond and atoms before/after bond to determine various torsions
	utility::vector1< core::id::AtomID > atomIDs;

	core::pose::Pose starting_pose_; //maintained from run to run

	std::string const InterfaceSasaDefinition_; //calculator name

	protocols::analysis::InterfaceAnalyzerMoverOP IAM_;

};

typedef utility::pointer::owning_ptr< UBQ_E2Mover > UBQ_E2MoverOP;

int main( int argc, char* argv[] )
{

	using basic::options::option;
	using namespace basic::options::OptionKeys;
 	option.add( UBQpdb, "ubiquitin structure" ).def("1UBQ.pdb");
 	option.add( E2pdb, "E2 structure" ).def("2OB4.pdb");
 	option.add( E2_residue, "E2 catalytic cysteine (PDB numbering)").def(85);
	option.add( SASAfilter, "filter out interface dSASA less than this").def(1000);
	option.add( scorefilter, "filter out total score greater than this").def(10);
	option.add( constraintweight, "atom pair cst weight").def(50);

	//initialize options
	devel::init(argc, argv);
	basic::prof_reset();

	if(basic::options::option[ basic::options::OptionKeys::in::file::s ].active()
		|| basic::options::option[ basic::options::OptionKeys::in::file::l ].active()
		|| basic::options::option[ basic::options::OptionKeys::in::file::silent ].active())
		utility_exit_with_message("do not use an input PDB with UBQ_E2 (program uses internally)");

	protocols::jd2::JobDistributor::get_instance()->go(new UBQ_E2Mover);

	basic::prof_show();
	TR << "************************d**o**n**e**************************************" << std::endl;

	return 0;
}
