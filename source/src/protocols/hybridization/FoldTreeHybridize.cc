// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file  protocols/hybridization/FoldTreeHybridize.cc
/// @brief Align a random jump to template
/// @author Yifan Song, David Kim

#include <protocols/hybridization/FoldTreeHybridize.hh>
#include <protocols/hybridization/FoldTreeHybridizeCreator.hh>
#include <protocols/hybridization/ChunkTrialMover.hh>
#include <protocols/hybridization/HybridizeFoldtreeDynamic.hh>
#include <protocols/hybridization/util.hh>
#include <protocols/hybridization/AllResiduesChanged.hh>
#include <protocols/hybridization/HybridizeSetup.hh>
#include <protocols/hybridization/DomainAssembly.hh>

#include <core/import_pose/import_pose.hh>

#include <core/pose/Pose.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/pose/PDBInfo.hh>
#include <core/pose/util.hh>
#include <core/sequence/Sequence.hh>
#include <core/sequence/util.hh>

#include <core/conformation/Conformation.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/conformation/util.hh>

#include <core/chemical/ResidueProperties.hh>
#include <core/chemical/ResidueTypeSet.hh>

#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/constraints/AtomPairConstraint.hh>
#include <core/scoring/constraints/BoundConstraint.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <core/scoring/methods/EnergyMethodOptions.hh>

#include <core/id/AtomID.hh>
#include <core/id/AtomID_Map.hh>
#include <core/util/kinematics_util.hh>
#include <core/fragment/ConstantLengthFragSet.hh>
#include <core/fragment/Frame.hh>
#include <core/fragment/FrameIterator.hh>
#include <core/fragment/util.hh>
#include <core/fragment/ConstantLengthFragSet.hh>

// symmetry
#include <core/pose/symmetry/util.hh>
#include <core/optimization/symmetry/SymAtomTreeMinimizer.hh>
#include <protocols/simple_moves/symmetry/SymPackRotamersMover.hh>
#include <core/conformation/symmetry/SymmetricConformation.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/PackerTask.hh>

#include <protocols/moves/MoverContainer.hh>
#include <protocols/moves/MonteCarlo.hh>

#include <protocols/simple_moves/rational_mc/RationalMonteCarlo.hh>
#include <protocols/simple_moves/MutateResidue.hh>
#include <protocols/moves/TrialMover.hh>
#include <protocols/simple_moves/GunnCost.hh>
#include <protocols/moves/RepeatMover.hh>
#include <protocols/moves/WhileMover.hh>

#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/loops_main.hh>
#include <protocols/loops/util.hh>

#include <ObjexxFCL/format.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/random/random.hh>
#include <numeric/random/random_permutation.hh>
#include <numeric/model_quality/rms.hh>
#include <numeric/model_quality/maxsub.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/OptionKeys.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/cm.OptionKeys.gen.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>
#include <basic/options/keys/rigid.OptionKeys.gen.hh>
#include <basic/options/keys/jumps.OptionKeys.gen.hh> // strand pairings
#include <basic/options/keys/evaluation.OptionKeys.gen.hh>
#include <basic/options/keys/abinitio.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

//add for cenrot
#include <basic/options/keys/corrections.OptionKeys.gen.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>
#include <protocols/simple_moves/SwitchResidueTypeSetMover.hh>
#include <core/pack/task/operation/TaskOperations.hh>

#include <numeric/random/DistributionSampler.hh>
#include <numeric/util.hh>

// parser
#include <protocols/rosetta_scripts/util.hh>
//#include <protocols/moves/DataMap.hh>
#include <utility/tag/Tag.hh>

// strand pairings
#include <core/kinematics/MoveMap.hh>
#include <core/fragment/SecondaryStructure.hh>
#include <protocols/jumping/JumpSetup.hh>
#include <protocols/simple_moves/ExtendedPoseMover.hh>
#include <protocols/jumping/SheetBuilder.hh>
#include <protocols/jumping/RandomSheetBuilder.hh>
#include <core/scoring/dssp/StrandPairing.hh>
#include <core/scoring/rms_util.hh>

#include <set>
#include <list>

static thread_local basic::Tracer TR( "protocols.hybridization.FoldTreeHybridize" );

namespace protocols {
namespace hybridization {

using namespace core;
using namespace chemical;
using namespace core::kinematics;
using namespace ObjexxFCL;
using namespace protocols::moves;
using namespace protocols::simple_moves;
using namespace protocols::loops;
using namespace numeric::model_quality;
using namespace id;
using namespace basic::options;
using namespace basic::options::OptionKeys;


FoldTreeHybridize::FoldTreeHybridize() :
		hybridize_setup_(NULL),
		foldtree_mover_()
{
	init();
}

FoldTreeHybridize::FoldTreeHybridize (
		core::Size const initial_template_index,
		utility::vector1 < core::pose::PoseOP > const & template_poses,
		utility::vector1 < core::Real > const & template_wts,
		utility::vector1 < protocols::loops::Loops > const & template_chunks,
		utility::vector1 < protocols::loops::Loops > const & template_contigs,
		core::fragment::FragSetOP fragments_small_in,
		core::fragment::FragSetOP fragments_big_in )
{
	init();

	//initialize template structures
	//  note: for strand pairings, templates that represent the pairs will be added to this
	//        and templates with incorrect pairs may be removed if filter_templates option is used
	initial_template_index_ = initial_template_index;
	template_poses_ = template_poses;
	template_wts_ = template_wts;
	template_chunks_ = template_chunks;
	template_contigs_ = template_contigs;

	// normalize weights
	normalize_template_wts();

	// abinitio frags
	frag_libs_small_.push_back(fragments_small_in);
	frag_libs_big_.push_back(fragments_big_in);
	frag_libs_1mer_.push_back( new core::fragment::ConstantLengthFragSet( 1 ) );
	chop_fragments( *frag_libs_small_[1], *frag_libs_1mer_[1] );
}

/// Extract information from a setup mover
void FoldTreeHybridize::setup_for_parser()
{
	initial_template_index_ = hybridize_setup_->initial_template_index();

	if (realign_domains_) {
		hybridize_setup_->realign_templates(hybridize_setup_->template_poses()[initial_template_index_]);
	}


	for (core::Size ipose=1; ipose<=hybridize_setup_->template_poses().size(); ++ipose) {
		template_poses_.push_back( new core::pose::Pose( *(hybridize_setup_->template_poses()[ipose]) ) );
	}
	template_wts_ = hybridize_setup_->template_wts();
	template_chunks_ = hybridize_setup_->template_chunks();
	template_contigs_ = hybridize_setup_->template_contigs();

	// normalize weights
	normalize_template_wts();

	// abinitio frags
	frag_libs_small_ = hybridize_setup_->fragments_small();
	frag_libs_big_ = hybridize_setup_->fragments_big();
	frag_libs_1mer_.push_back( new core::fragment::ConstantLengthFragSet( 1 ) );
	chop_fragments( *frag_libs_small_[1], *frag_libs_1mer_[1] );

	std::string cst_fn = hybridize_setup_->template_cst_fn()[initial_template_index_];
	set_constraint_file( cst_fn );
	set_domain_assembly( hybridize_setup_->domain_assembly() );
	set_add_hetatm( hybridize_setup_->add_hetatm(), hybridize_setup_->hetatm_self_cst_weight(), hybridize_setup_->hetatm_prot_cst_weight() );
}

void
FoldTreeHybridize::init() {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	increase_cycles_ = option[cm::hybridize::stage1_increase_cycles]();
	stage1_1_cycles_ = option[cm::hybridize::stage1_1_cycles]();
	stage1_2_cycles_ = option[cm::hybridize::stage1_2_cycles]();
	stage1_3_cycles_ = option[cm::hybridize::stage1_3_cycles]();
	stage1_4_cycles_ = option[cm::hybridize::stage1_4_cycles]();
	add_non_init_chunks_ = option[cm::hybridize::add_non_init_chunks]();
	frag_weight_aligned_ = option[cm::hybridize::frag_weight_aligned]();
	auto_frag_insertion_weight_ = option[cm::hybridize::auto_frag_insertion_weight]();
	max_registry_shift_ = option[cm::hybridize::max_registry_shift]();

	initialize_pose_by_templates_ = true;
	realign_domains_ = true;

	frag_1mer_insertion_weight_ = 0.0;
	small_frag_insertion_weight_ = 0.0;
	big_frag_insertion_weight_ = 0.50;
	chunk_insertion_weight_ = 5.;

	top_n_big_frag_ = 25;
	top_n_small_frag_ = 200;
	domain_assembly_ = false;
	add_hetatm_ = false;
	hetatm_self_cst_weight_ = 10.;
	hetatm_prot_cst_weight_ = 0.;
	// default scorefunction
	set_scorefunction ( core::scoring::ScoreFunctionFactory::create_score_function( "score3" ) );

	// strand pairings
	pairings_file_ = "";
	sheets_.push_back(1);
	random_sheets_.push_back(1);
	filter_templates_ = false;

	overlap_chainbreaks_ = option[ jumps::overlap_chainbreak ](); // default is true

	// evaluation
	// native
	if ( option[ in::file::native ].user() ) {
		native_ = new core::pose::Pose;
		core::import_pose::pose_from_pdb( *native_, option[ in::file::native ]() );
	} else if ( option[ evaluation::align_rmsd_target ].user() ) {
		native_ = new core::pose::Pose;
		utility::vector1< std::string > const & align_rmsd_target( option[ evaluation::align_rmsd_target ]() );
		core::import_pose::pose_from_pdb( *native_, align_rmsd_target[1] ); // just use the first one for now
	}

}

void
FoldTreeHybridize::normalize_template_wts() {
	// normalize weights
	core::Real weight_sum = 0.0;
	for (Size i=1; i<=template_poses_.size(); ++i) weight_sum += template_wts_[i];
	for (Size i=1; i<=template_poses_.size(); ++i) template_wts_[i] /= weight_sum;
}

void FoldTreeHybridize::set_task_factory( core::pack::task::TaskFactoryOP task_factory_in ) {
 	  task_factory_ = task_factory_in;
}

void
FoldTreeHybridize::set_loops_to_virt_ala(core::pose::Pose & pose, Loops loops)
{
	chemical::ResidueTypeSet const& restype_set( pose.residue(1).residue_type_set() );

	for (Size iloop=1; iloop<=loops.num_loop(); ++iloop) {
		for (Size ires=loops[iloop].start(); ires<=loops[iloop].stop(); ++ires) {

			// Create the new residue and replace it
			conformation::ResidueOP new_res = conformation::ResidueFactory::create_residue(
																							 restype_set.name_map("VBB"), pose.residue(ires),
																							 pose.conformation());
			// Make sure we retain as much info from the previous res as possible
			conformation::copy_residue_coordinates_and_rebuild_missing_atoms( pose.residue(ires),
																			 *new_res, pose.conformation() );
			pose.replace_residue(ires, *new_res, false );
			//core::pose::add_variant_type_to_pose_residue(pose, "VIRTUAL_BB", ires);
		}
	}
}

void
FoldTreeHybridize::revert_loops_to_original(core::pose::Pose & pose, Loops loops)
{
	std::string sequence = core::sequence::read_fasta_file( option[ in::file::fasta ]()[1] )[1]->sequence();

	for (Size iloop=1; iloop<=loops.num_loop(); ++iloop) {
		for (Size ires=loops[iloop].start(); ires<=loops[iloop].stop(); ++ires) {
			utility::vector1< std::string > variant_types = pose.residue_type(ires).properties().get_list_of_variants();
			MutateResidue mutate_mover(ires, sequence[ires-1]);
			mutate_mover.apply(pose);
			for (Size i_var = 1; i_var <=variant_types.size(); ++i_var) {
				core::pose::add_variant_type_to_pose_residue(pose,
						core::chemical::ResidueProperties::get_variant_from_string( variant_types[i_var] ), ires);
			}
		}
	}
}


Real
FoldTreeHybridize::gap_distance(Size Seq_gap)
{
	core::Real gap_torr_0( 4.0);
	core::Real gap_torr_1( 7.5);
	core::Real gap_torr_2(11.0);
	core::Real gap_torr_3(14.5);
	core::Real gap_torr_4(18.0);
	core::Real gap_torr_5(21.0);
	core::Real gap_torr_6(24.5);
	core::Real gap_torr_7(27.5);
	core::Real gap_torr_8(31.0);

	switch (Seq_gap) {
		case 0:
			return gap_torr_0; break;
		case 1:
			return gap_torr_1; break;
		case 2:
			return gap_torr_2; break;
		case 3:
			return gap_torr_3; break;
		case 4:
			return gap_torr_4; break;
		case 5:
			return gap_torr_5; break;
		case 6:
			return gap_torr_6; break;
		case 7:
			return gap_torr_7; break;
		case 8:
			return gap_torr_8; break;
		default:
			return 9999.;
	}
	return 9999.;
}

void FoldTreeHybridize::add_gap_constraints_to_pose(core::pose::Pose & pose, Loops const & chunks, int gap_edge_shift, Real stdev) {
	using namespace ObjexxFCL::format;
	for (Size i=1; i<chunks.num_loop(); ++i) {
		int gap_start = chunks[i].stop()	+ gap_edge_shift;
		int gap_stop  = chunks[i+1].start() - gap_edge_shift;
		int gap_size = gap_stop - gap_start - 1;
		if (gap_size < 0) continue;
		if (gap_size > 8) continue;
		if (!pose.residue_type(gap_start).is_protein()) continue;
		if (!pose.residue_type(gap_stop ).is_protein()) continue;
		Size iatom = pose.residue_type(gap_start).atom_index("CA");
		Size jatom = pose.residue_type(gap_stop ).atom_index("CA");

		TR << "Add constraint to residue " << I(4,gap_start) << " and residue " << I(4,gap_stop) << std::endl;
		pose.add_constraint(
							new core::scoring::constraints::AtomPairConstraint(
																				 core::id::AtomID(iatom,gap_start),
																				 core::id::AtomID(jatom,gap_stop),
																				 new core::scoring::constraints::BoundFunc( 0, gap_distance(gap_size), stdev, "gap" ) )
							);

	}
}

void
FoldTreeHybridize::setup_foldtree(core::pose::Pose & pose) {
	// Add secondary structure information to the pose. Because the number of residues
	// in the pose and secondary structure differ (because of virtual residues), we need
	// to compute this quantity explicitly.
	core::Size num_residues_nonvirt = get_num_residues_nonvirt(pose);

	bool ok = false;
	if ( option[ OptionKeys::in::file::psipred_ss2 ].user() )
		ok = set_secstruct_from_psipred_ss2(pose);
	if (!ok) {
		core::fragment::SecondaryStructureOP ss_def = new core::fragment::SecondaryStructure( *frag_libs_small_[1], num_residues_nonvirt, false );
		for ( core::Size i = 1; i<= num_residues_nonvirt; ++i ) {
			pose.set_secstruct( i, ss_def->secstruct(i) );
		}
		TR.Info << "Secondary structure from fragments: " << pose.secstruct() << std::endl;
	} else {
		TR.Info << "Secondary structure from psipred_ss2: " << pose.secstruct() << std::endl;
	}

	// combine:
	// (a) contigs in the current template

	core::Size nres = pose.total_residue();

	//symmetry
	core::conformation::symmetry::SymmetryInfoCOP symm_info;
	if ( core::pose::symmetry::is_symmetric(pose) ) {
		core::conformation::symmetry::SymmetricConformation & SymmConf (
			dynamic_cast<core::conformation::symmetry::SymmetricConformation &> ( pose.conformation()) );
		symm_info = SymmConf.Symmetry_Info();
		nres = symm_info->num_independent_residues();
	}

	utility::vector1< bool > template_mask( nres, false );
	protocols::loops::Loops my_chunks(template_chunks_[initial_template_index_]);

	for (core::Size i = 1; i<=template_chunks_[initial_template_index_].num_loop(); ++i) {
		Size seqpos_start_target = template_poses_[initial_template_index_]->pdb_info()->number(
			template_chunks_[initial_template_index_][i].start());
		my_chunks[i].set_start( seqpos_start_target );
		Size seqpos_stop_target = template_poses_[initial_template_index_]->pdb_info()->number(
			template_chunks_[initial_template_index_][i].stop());
		my_chunks[i].set_stop( seqpos_stop_target );
		for (Size j=seqpos_start_target; j<=seqpos_stop_target; ++j) template_mask[j] = true;
	}

	// strand pairings

	// keep track of strand pairs
	strand_pairs_.clear();
	std::set<core::Size> strand_pair_library_positions; // chunk positions from the strand pair library

	// add strand pairing chunks that are missing from the initial template
	// this is mainly for the dynamic fold tree builder
	// note: this adds to my_chunks
	std::set<core::Size>::iterator pairings_iter;
	for (pairings_iter = strand_pairings_template_indices_.begin(); pairings_iter != strand_pairings_template_indices_.end(); ++pairings_iter) {
		Size seqpos_pairing_i = template_poses_[*pairings_iter]->pdb_info()->number(template_chunks_[*pairings_iter][1].start());
		Size seqpos_pairing_j = template_poses_[*pairings_iter]->pdb_info()->number(template_chunks_[*pairings_iter][2].start());
		strand_pairs_.push_back( std::pair< core::Size, core::Size >( seqpos_pairing_i, seqpos_pairing_j ) );
		if (!template_mask[seqpos_pairing_i]) {
			protocols::loops::Loop new_loop_i = template_chunks_[*pairings_iter][1];
			new_loop_i.set_start( seqpos_pairing_i );
			new_loop_i.set_stop( seqpos_pairing_i );
			my_chunks.add_loop( new_loop_i );
			strand_pair_library_positions.insert(seqpos_pairing_i);
			template_mask[seqpos_pairing_i] = true;
		}
		if (!template_mask[seqpos_pairing_j]) {
			protocols::loops::Loop new_loop_j = template_chunks_[*pairings_iter][2];
			new_loop_j.set_start( seqpos_pairing_j );
			new_loop_j.set_stop( seqpos_pairing_j );
			my_chunks.add_loop( new_loop_j );
			strand_pair_library_positions.insert(seqpos_pairing_j);
			template_mask[seqpos_pairing_j] = true;
		}
	} // strand pairings


	TR.Debug << "Chunks of initial template: " << initial_template_index_ << std::endl;
	TR.Debug << template_chunks_[initial_template_index_] << std::endl;
	TR.Debug << "Chunks from initial template: " << std::endl;
	TR.Debug << my_chunks << std::endl;

	if ( add_non_init_chunks_ || domain_assembly_ ) {
		// (b) probabilistically sampled chunks from all other templates _outside_ these residues
		utility::vector1< std::pair< core::Real, protocols::loops::Loop > >  wted_insertions_to_consider;
		for (core::Size itempl = 1; itempl<=template_chunks_.size(); ++itempl) {
			if (itempl == initial_template_index_) continue;
			for (core::Size icontig = 1; icontig<=template_chunks_[itempl].num_loop(); ++icontig) {
				// remap
				Size seqpos_start_target = template_poses_[itempl]->pdb_info()->number(template_chunks_[itempl][icontig].start());
				Size seqpos_stop_target = template_poses_[itempl]->pdb_info()->number(template_chunks_[itempl][icontig].stop());

				bool uncovered = true;
				for (Size j=seqpos_start_target; j<=seqpos_stop_target && uncovered ; ++j)
					uncovered &= !template_mask[j];

				if (uncovered) {
					protocols::loops::Loop new_loop = template_chunks_[itempl][icontig];
					new_loop.set_start( seqpos_start_target );
					new_loop.set_stop( seqpos_stop_target );
					wted_insertions_to_consider.push_back( std::pair< core::Real, protocols::loops::Loop >( template_wts_[itempl] , new_loop ) );
				}
			}
		}

		// (c) randomly shuffle, then add each with given prob
		TR.Debug << "Chunks from all template: " << std::endl;
		TR.Debug << my_chunks << std::endl;
		std::random_shuffle ( wted_insertions_to_consider.begin(), wted_insertions_to_consider.end() );
		for (Size i=1; i<=wted_insertions_to_consider.size(); ++i) {
			// ensure the insert is still valid
			bool uncovered = true;
			for (Size j=wted_insertions_to_consider[i].second.start(); j<=wted_insertions_to_consider[i].second.stop() && uncovered ; ++j)
				uncovered &= !template_mask[j];

			if (!uncovered) continue;

			core::Real selector = numeric::random::uniform();
			if (domain_assembly_) selector = 0.; // always add additional domain if in domain assembly
			TR << "Consider " << wted_insertions_to_consider[i].second.start() << "," << wted_insertions_to_consider[i].second.stop() << std::endl;
			if (selector <= wted_insertions_to_consider[i].first) {
				TR << " ====> taken!" << std::endl;
				my_chunks.add_loop( wted_insertions_to_consider[i].second );
				for (Size j=wted_insertions_to_consider[i].second.start(); j<=wted_insertions_to_consider[i].second.stop(); ++j) {
					template_mask[j] = true;
				}
			}
		}
	}

	my_chunks.sequential_order();
	TR << "Chunks used for foldtree setup: " << std::endl;
	TR << my_chunks << std::endl;

	foldtree_mover_.initialize(pose, my_chunks, strand_pairs_, strand_pair_library_positions);
	TR << pose.fold_tree() << std::endl;
}


numeric::xyzVector<Real>
FoldTreeHybridize::center_of_mass(core::pose::Pose const & pose) {
	int  nres( pose.total_residue() ), nAtms = 0;
	numeric::xyzVector<core::Real> massSum(0.,0.,0.), CoM;
	for ( int i=1; i<= nres; ++i ) {
		conformation::Residue const & rsd( pose.residue(i) );
		if (rsd.aa() == core::chemical::aa_vrt) continue;

		for ( Size j=1; j<= rsd.nheavyatoms(); ++j ) {
			conformation::Atom const & atom( rsd.atom(j) );
			massSum += atom.xyz();
			nAtms++;
		}
	}
	CoM = massSum / (core::Real)nAtms;
	return CoM;
}

void
FoldTreeHybridize::translate_virt_to_CoM(core::pose::Pose & pose) {
	numeric::xyzVector<Real> CoM;
	CoM = center_of_mass(pose);
	numeric::xyzVector<Real> curr_pos = pose.residue(pose.total_residue()).xyz(1);
	numeric::xyzVector<Real> translation = CoM - curr_pos;

	using namespace ObjexxFCL::format;
	TR.Debug << F(8,3,translation.x()) << F(8,3,translation.y()) << F(8,3,translation.z()) << std::endl;

	// apply transformation
	utility::vector1< core::id::AtomID > ids;
	utility::vector1< numeric::xyzVector<core::Real> > positions;
	for (Size iatom = 1; iatom <= pose.residue_type(pose.total_residue()).natoms(); ++iatom) {
		numeric::xyzVector<core::Real> atom_xyz = pose.xyz( core::id::AtomID(iatom,pose.total_residue()) );
		ids.push_back(core::id::AtomID(iatom,pose.total_residue()));
		positions.push_back(atom_xyz + translation);
	}
	pose.batch_set_xyz(ids,positions);
}


utility::vector1< core::Real > FoldTreeHybridize::get_residue_weights_for_big_frags(core::pose::Pose & pose) {
	using namespace ObjexxFCL::format;
	core::Size num_residues_nonvirt = get_num_residues_nonvirt(pose);
	utility::vector1< core::Real > residue_weights(num_residues_nonvirt, 0.0);
	TR.Debug << "Fragment insertion positions and weights:" << std::endl;
	for ( Size ires=1; ires<= num_residues_nonvirt; ++ires ) {
        if (!residue_sample_abinitio_[ires]) {
            residue_weights[ires] = 0.0;
            continue;
        }

		if (domain_assembly_) {
			bool residue_in_template = false;
			for (Size i_template=1; i_template<=template_poses_.size(); ++i_template) {
				protocols::loops::Loops renumbered_template_chunks = renumber_with_pdb_info(
										template_contigs_[i_template], template_poses_[i_template]);
				if (renumbered_template_chunks.has(ires)) {
					residue_in_template = true;
					break;
				}
			}
			if (! residue_in_template ) {
				residue_weights[ires] = 1.0;
			}
			else {
				residue_weights[ires] = frag_weight_aligned_;
			}
		}
		else {
			protocols::loops::Loops renumbered_template_chunks
			= renumber_with_pdb_info(
										 template_contigs_[initial_template_index_], template_poses_[initial_template_index_]);

            if (residue_sample_abinitio_[ires]) {
			if (! renumbered_template_chunks.has(ires) ) {
                    residue_weights[ires] = 1.0;
			}
			else {
				residue_weights[ires] = frag_weight_aligned_;
			}
            }
		}
		TR.Debug << " " << ires << ": " << F(7,5,residue_weights[ires]) << std::endl;
 }

	// reset linker fragment insertion weights
	if (domain_assembly_) {
		for (Size i_template=1; i_template<=template_poses_.size(); ++i_template) {
			int coverage_start = template_poses_[i_template]->pdb_info()->number(1);
			int coverage_end   = template_poses_[i_template]->pdb_info()->number(1);
			for (Size ires = 1; ires <= template_poses_[i_template]->total_residue(); ++ires) {
				if (template_poses_[i_template]->pdb_info()->number(ires) < coverage_start) {
					coverage_start = template_poses_[i_template]->pdb_info()->number(ires);
				}
				if (template_poses_[i_template]->pdb_info()->number(ires) > coverage_end) {
					coverage_end = template_poses_[i_template]->pdb_info()->number(ires);
				}
			}

			for (int shift = -5; shift<=5; ++shift) {
				int ires = coverage_start + shift;
				if (ires >= 1 && ires <= (int)num_residues_nonvirt) {
					residue_weights[ires] = 1.;
					TR.Debug << " " << ires << ": " << F(7,5,residue_weights[ires]) << std::endl;
				}

				ires = coverage_end + shift;
				if (ires >= 1 && ires <= (int)num_residues_nonvirt) {
					residue_weights[ires] = 1.;
					TR.Debug << " " << ires << ": " << F(7,5,residue_weights[ires]) << std::endl;
				}
			}
		}
	}
	return residue_weights;
}

utility::vector1< core::Size > FoldTreeHybridize::get_jump_anchors() {
	std::set<core::Size>::iterator iter;
	utility::vector1< core::Size > jump_anchors = foldtree_mover_.get_anchors();
	std::set< core::Size > unique;
	for (core::Size i = 1; i<=jump_anchors.size();++i) unique.insert(jump_anchors[i]);
	jump_anchors.clear();
	// ignore strand pair residues
	std::set<core::Size> strand_pair_residues = get_pairings_residues();
	for (iter = unique.begin(); iter != unique.end(); ++iter) {
		if (strand_pair_residues.count(*iter)) continue;
		jump_anchors.push_back(*iter);
	}
	return jump_anchors;
}

utility::vector1< core::Real > FoldTreeHybridize::get_residue_weights_for_1mers(core::pose::Pose & pose) {
  using namespace ObjexxFCL::format;
	core::Size num_residues_nonvirt = get_num_residues_nonvirt(pose);
  utility::vector1< core::Real > residue_weights( get_residue_weights_for_big_frags(pose) );
  utility::vector1< core::Real > residue_weights_new( num_residues_nonvirt, 0.0 );
  utility::vector1< core::Size > jump_anchors = get_jump_anchors();
  core::Size min_small_frag_len = 999999;
  TR.Debug << "1mer fragment insertion positions and weights:" << std::endl;
	for (core::Size i = 1; i<=frag_libs_small_.size(); ++i) {
		if (frag_libs_small_[i]->max_frag_length() < min_small_frag_len) {
			min_small_frag_len = frag_libs_small_[i]->max_frag_length();
		}
	}

  core::Size last_anchor = 0;
  for (core::Size i = 1; i<=jump_anchors.size(); ++i) {
    core::Size anchor_gap = jump_anchors[i]-last_anchor-1;
    if (anchor_gap && (anchor_gap < min_small_frag_len)) {
      for (core::Size ipos = last_anchor+1;ipos<jump_anchors[i];++ipos) {
          if (residue_sample_abinitio_[ipos]) {
        residue_weights_new[ipos] = residue_weights[ipos];
        TR.Debug << " " << ipos << ": " << F(7,5,residue_weights[ipos]) << std::endl;
          }
      }
    }
    last_anchor = jump_anchors[i];
  }
  core::Size anchor_gap = num_residues_nonvirt-last_anchor;
  if (anchor_gap && (anchor_gap < min_small_frag_len)) {
    for (core::Size ipos = last_anchor+1;ipos<=num_residues_nonvirt;++ipos) {
        if (residue_sample_abinitio_[ipos]) {
      residue_weights_new[ipos] = residue_weights[ipos];
      TR.Debug << " " << ipos << ": " << F(7,5,residue_weights[ipos]) << std::endl;
        }
    }
  }
  return residue_weights_new;
}

utility::vector1< core::Real > FoldTreeHybridize::get_residue_weights_for_small_frags(core::pose::Pose & pose) {
	using namespace ObjexxFCL::format;
	core::Size num_residues_nonvirt = get_num_residues_nonvirt(pose);
	utility::vector1< core::Real > residue_weights( get_residue_weights_for_big_frags(pose) );
	utility::vector1< core::Real > residue_weights_new( num_residues_nonvirt, 0.0 );
	utility::vector1< core::Size > jump_anchors = get_jump_anchors();
	core::Size min_big_frag_len = 999999;
	TR.Debug << "Small fragment insertion positions and weights:" << std::endl;
	for (core::Size i = 1; i<=frag_libs_big_.size(); ++i) {
		if (frag_libs_big_[i]->max_frag_length() < min_big_frag_len) {
			min_big_frag_len = frag_libs_big_[i]->max_frag_length();
        }
    }
	core::Size last_anchor = 0;
	for (core::Size i = 1; i<=jump_anchors.size(); ++i) {
		core::Size anchor_gap = jump_anchors[i]-last_anchor-1;
		if (anchor_gap && anchor_gap < min_big_frag_len) {
			for (core::Size ipos = last_anchor+1;ipos<jump_anchors[i];++ipos) {
                if (residue_sample_abinitio_[ipos]) {
				residue_weights_new[ipos] = residue_weights[ipos];
				TR.Debug << " " << ipos << ": " << F(7,5,residue_weights[ipos]) << std::endl;
                }
			}
		}
		last_anchor = jump_anchors[i];
	}
	core::Size anchor_gap = num_residues_nonvirt-last_anchor;
	if (anchor_gap && anchor_gap < min_big_frag_len) {
		for (core::Size ipos = last_anchor+1;ipos<=num_residues_nonvirt;++ipos) {
            if (residue_sample_abinitio_[ipos]) {
			residue_weights_new[ipos] = residue_weights[ipos];
			TR.Debug << " " << ipos << ": " << F(7,5,residue_weights[ipos]) << std::endl;
            }
		}
	}
	return residue_weights_new;
}

void FoldTreeHybridize::restore_original_foldtree(core::pose::Pose & pose) {
	foldtree_mover_.reset(pose);
}

void
FoldTreeHybridize::setup_scorefunctions(
		core::scoring::ScoreFunctionOP score0,
		core::scoring::ScoreFunctionOP score1,
		core::scoring::ScoreFunctionOP score2,
		core::scoring::ScoreFunctionOP score5,
		core::scoring::ScoreFunctionOP score3) {
	core::Real lincb_orig = scorefxn_->get_weight( core::scoring::linear_chainbreak );
	core::Real cst_orig = scorefxn_->get_weight( core::scoring::atom_pair_constraint );

	score0->reset();
	score0->set_weight( core::scoring::vdw, 0.1*scorefxn_->get_weight( core::scoring::vdw ) );
	score0->set_weight( core::scoring::elec_dens_fast, scorefxn_->get_weight( core::scoring::elec_dens_fast ) );
	score0->set_weight( core::scoring::coordinate_constraint, scorefxn_->get_weight( core::scoring::coordinate_constraint ) );

	score1->reset();
	score1->set_weight( core::scoring::linear_chainbreak, 0.1*lincb_orig );
	score1->set_weight( core::scoring::atom_pair_constraint, 0.1*cst_orig );
	score1->set_weight( core::scoring::vdw, scorefxn_->get_weight( core::scoring::vdw ) );
	score1->set_weight( core::scoring::env, scorefxn_->get_weight( core::scoring::env ) );
	score1->set_weight( core::scoring::cen_env_smooth, scorefxn_->get_weight( core::scoring::cen_env_smooth ) );
	score1->set_weight( core::scoring::pair, scorefxn_->get_weight( core::scoring::pair ) );
	score1->set_weight( core::scoring::cen_pair_smooth, scorefxn_->get_weight( core::scoring::cen_pair_smooth ) );
	score1->set_weight( core::scoring::hs_pair, scorefxn_->get_weight( core::scoring::hs_pair ) );
	score1->set_weight( core::scoring::ss_pair, 0.3*scorefxn_->get_weight( core::scoring::ss_pair ) );
	score1->set_weight( core::scoring::sheet, scorefxn_->get_weight( core::scoring::sheet ) );
	score1->set_weight( core::scoring::elec_dens_fast, scorefxn_->get_weight( core::scoring::elec_dens_fast ) );
	score1->set_weight( core::scoring::coordinate_constraint, scorefxn_->get_weight( core::scoring::coordinate_constraint ) );
	//STRAND_STRAND_WEIGHTS 1 11
	core::scoring::methods::EnergyMethodOptions score1_options(score1->energy_method_options());
	score1_options.set_strand_strand_weights(1,11);
	score1->set_energy_method_options(score1_options);


	score2->reset();
	score2->set_weight( core::scoring::linear_chainbreak, 0.25*lincb_orig );
	score2->set_weight( core::scoring::atom_pair_constraint, 0.25*cst_orig );
	score2->set_weight( core::scoring::vdw, scorefxn_->get_weight( core::scoring::vdw ) );
	score2->set_weight( core::scoring::env, scorefxn_->get_weight( core::scoring::env ) );
	score2->set_weight( core::scoring::cen_env_smooth, scorefxn_->get_weight( core::scoring::cen_env_smooth ) );
	score2->set_weight( core::scoring::cbeta, 0.25*scorefxn_->get_weight( core::scoring::cbeta ) );
	score2->set_weight( core::scoring::cbeta_smooth, 0.25*scorefxn_->get_weight( core::scoring::cbeta_smooth ) );
	score2->set_weight( core::scoring::cenpack, 0.5*scorefxn_->get_weight( core::scoring::cenpack ) );
	score2->set_weight( core::scoring::cenpack_smooth, 0.5*scorefxn_->get_weight( core::scoring::cenpack_smooth ) );
	score2->set_weight( core::scoring::pair, scorefxn_->get_weight( core::scoring::pair ) );
	score2->set_weight( core::scoring::cen_pair_smooth, scorefxn_->get_weight( core::scoring::cen_pair_smooth ) );
	score2->set_weight( core::scoring::hs_pair, scorefxn_->get_weight( core::scoring::hs_pair ) );
	score2->set_weight( core::scoring::ss_pair, 0.3*scorefxn_->get_weight( core::scoring::ss_pair ) );
	score2->set_weight( core::scoring::sheet, scorefxn_->get_weight( core::scoring::sheet ) );
	score2->set_weight( core::scoring::elec_dens_fast, scorefxn_->get_weight( core::scoring::elec_dens_fast ) );
	score2->set_weight( core::scoring::coordinate_constraint, scorefxn_->get_weight( core::scoring::coordinate_constraint ) );
	//STRAND_STRAND_WEIGHTS 1 6
	core::scoring::methods::EnergyMethodOptions score2_options(score1->energy_method_options());
	score2_options.set_strand_strand_weights(1,6);
	score2->set_energy_method_options(score2_options);

	score5->reset();
	score5->set_weight( core::scoring::linear_chainbreak, 0.25*lincb_orig );
	score5->set_weight( core::scoring::atom_pair_constraint, 0.25*cst_orig );
	score5->set_weight( core::scoring::vdw, scorefxn_->get_weight( core::scoring::vdw ) );
	score5->set_weight( core::scoring::env, scorefxn_->get_weight( core::scoring::env ) );
	score5->set_weight( core::scoring::cen_env_smooth, scorefxn_->get_weight( core::scoring::cen_env_smooth ) );
	score5->set_weight( core::scoring::cbeta, 0.25*scorefxn_->get_weight( core::scoring::cbeta ) );
	score5->set_weight( core::scoring::cbeta_smooth, 0.25*scorefxn_->get_weight( core::scoring::cbeta_smooth ) );
	score5->set_weight( core::scoring::cenpack, 0.5*scorefxn_->get_weight( core::scoring::cenpack ) );
	score5->set_weight( core::scoring::cenpack_smooth, 0.5*scorefxn_->get_weight( core::scoring::cenpack_smooth ) );
	score5->set_weight( core::scoring::pair, scorefxn_->get_weight( core::scoring::pair ) );
	score5->set_weight( core::scoring::cen_pair_smooth, scorefxn_->get_weight( core::scoring::cen_pair_smooth ) );
	score5->set_weight( core::scoring::hs_pair, scorefxn_->get_weight( core::scoring::hs_pair ) );
	score5->set_weight( core::scoring::ss_pair, 0.3*scorefxn_->get_weight( core::scoring::ss_pair ) );
	score5->set_weight( core::scoring::sheet, scorefxn_->get_weight( core::scoring::sheet ) );
	score5->set_weight( core::scoring::elec_dens_fast, scorefxn_->get_weight( core::scoring::elec_dens_fast ) );
	score5->set_weight( core::scoring::coordinate_constraint, scorefxn_->get_weight( core::scoring::coordinate_constraint ) );
	//STRAND_STRAND_WEIGHTS 1 11
	core::scoring::methods::EnergyMethodOptions score5_options(score1->energy_method_options());
	score5_options.set_strand_strand_weights(1,11);
	score5->set_energy_method_options(score5_options);

	score3 = scorefxn_->clone();
}



// start strand pairings methods

void
FoldTreeHybridize::add_strand_pairings() {
	if (pairings_file_.length()==0) return;

// 1. read in the pairings file and get a jump sample (pairing jumps and fragments)
//    A jump sample is a set of pairings from the pairing file that has either
//    sheets or random sheets number of pairings.
//    The code is from protocols/jumping.

	TR << "read pairings file: " << pairings_file_ << std::endl;

	core::scoring::dssp::PairingsList pairings;
	read_pairing_list( pairings_file_, pairings );

	// get sheet-topology
	//  this code is taken from protocols/jumping and protocols/abinitio
	core::fragment::SecondaryStructureOP ss_def = new core::fragment::SecondaryStructure( *frag_libs_small_[1], false ); // get SS from frags
	protocols::jumping::BaseJumpSetupOP jump_def;
	if (sheets_.size()) {
		jump_def = new protocols::jumping::SheetBuilder( ss_def, pairings, sheets_ );
	} else {
		jump_def = new protocols::jumping::RandomSheetBuilder( ss_def, pairings, random_sheets_ );
	}
	core::Size attempts( 10 );
	do {
		jump_sample_ = jump_def->create_jump_sample();
	} while ( !jump_sample_.is_valid() && attempts-- );
	if ( !jump_sample_.is_valid() ) {
		utility_exit_with_message( "not able to get jump sample in HybridizeProtocol::add_strand_pairings()" );
	}
	TR << "jump_sample: " << jump_sample_ << std::endl; // debug

	// generate the jump fragments
	core::kinematics::MoveMapOP movemap = new core::kinematics::MoveMap;
	movemap->set_bb( true );
	movemap->set_jump( true );
	jump_frags_ = jump_def->generate_jump_frags( jump_sample_, *movemap );

// 2. identify templates with incorrect pairings
	utility::vector1< core::scoring::dssp::Pairing > pairings_to_add;
	ObjexxFCL::FArray2D_int jumps = jump_sample_.jumps();
	for ( core::Size i = 1; i <= jump_sample_.size(); ++i ) {
		core::Size jumpres1 = jumps(1,i), jumpres2 = jumps(2,i);
		core::scoring::dssp::Pairing pairing = jump_sample_.get_pairing( jumpres1, jumpres2 );
		if (!pairing.Pos1() || !pairing.Pos2())
			utility_exit_with_message( "not able to get pairing " + ObjexxFCL::string_of(jumpres1) + " " +
					ObjexxFCL::string_of(jumpres2) + " from jump sample in HybridizeProtocol::add_strand_pairings()" );
		pairings_to_add.push_back( pairing );
		for (core::Size itempl = 1; itempl<=template_chunks_.size(); ++itempl) {
			protocols::loops::Loops renumbered_template_chunks = renumber_with_pdb_info(
				template_chunks_[itempl], template_poses_[itempl]);
			core::scoring::dssp::StrandPairingSet strand_pairings( *template_poses_[itempl] );
			if (renumbered_template_chunks.has(jumpres1) && renumbered_template_chunks.has(jumpres2)) {
				// check template pose for correct pairing
				if (!strand_pairings.has_pairing( pairing )) {
					TR << "WARNING! template " << itempl << " is missing pairing: " << pairing << std::endl; // pleating may be different
					templates_with_incorrect_strand_pairings_.insert(itempl);
				} else {
					TR << "template " << itempl << " has pairing: " << pairing << std::endl; // pleating may be different
				}
			}
		}
	}
	// remove templates with incorrect pairings if desired
	if (filter_templates_ && templates_with_incorrect_strand_pairings_.size()) {
		filter_templates(templates_with_incorrect_strand_pairings_);
		templates_with_incorrect_strand_pairings_.clear();
	}

// 3. add chunks from pairings (must do after filtering templates for correct indices)
	for ( core::Size i = 1; i <= pairings_to_add.size(); ++i )
		add_strand_pairing( pairings_to_add[i] );

}


void
FoldTreeHybridize::add_strand_pairing(
	core::scoring::dssp::Pairing const & pairing
)
{
	protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();

	// create a pose with the strand pairing
	core::pose::PoseOP pairing_pose = new core::pose::Pose();
	protocols::simple_moves::ExtendedPoseMover m(target_sequence_);
	m.apply(*pairing_pose);
	pairing_pose->fold_tree( jump_sample_.fold_tree() );
	jump_mover->apply_at_all_positions( *pairing_pose );
	ObjexxFCL::FArray2D_int jumps( 2, 1 );
	jumps(1, 1) = 1;
	jumps(2, 1) = 2;
	ObjexxFCL::FArray1D_int cuts(1);
	cuts(1) = 1;
	core::kinematics::FoldTree pairing_fold_tree;
	if (!pairing_fold_tree.tree_from_jumps_and_cuts( 2, 1, jumps, cuts, 1, true /* verbose */ )) {
		TR << "WARNING! tree_from_jumps_and_cuts failed for pairing " << pairing << " so skipping it." << std::endl;
		return;
	}
	utility::vector1< core::Size > pairing_positions;
	pairing_positions.push_back(pairing.Pos1());
	pairing_positions.push_back(pairing.Pos2());
	core::pose::PoseOP trimmed_pairing_pose = new core::pose::Pose();
	core::pose::create_subpose( *pairing_pose, pairing_positions, pairing_fold_tree, *trimmed_pairing_pose );
	// add correct resnums to PDBInfo
	utility::vector1< int > pdb_numbering;
	pdb_numbering.push_back(pairing.Pos1());
	pdb_numbering.push_back(pairing.Pos2());
	core::pose::PDBInfoOP pdb_info( new core::pose::PDBInfo( trimmed_pairing_pose->total_residue() ) );
	pdb_info->set_numbering( pdb_numbering );
	pdb_info->set_chains( ' ' );
	pdb_info->obsolete( false );
	trimmed_pairing_pose->set_secstruct( 1, 'E' );
	trimmed_pairing_pose->set_secstruct( 2, 'E' );
	//std::string pairing_pdb = "pairing_" + ObjexxFCL::string_of(pairing.Pos1()) + "_" + ObjexxFCL::string_of(pairing.Pos2()) + ".pdb";
	//trimmed_pairing_pose->dump_pdb(pairing_pdb); // debug, do this before setting pdb_info or it will seg fault
	trimmed_pairing_pose->pdb_info( pdb_info );

	// add pairing as a template
	protocols::loops::Loops contigs = protocols::loops::extract_continuous_chunks(*trimmed_pairing_pose, 1);
	// add just the first residue chunk if there are no templates (if all templates were filtered out)
	if (!template_poses_.size()) {
		template_wts_.push_back( 0.0 ); // should not be used
		template_poses_.push_back( trimmed_pairing_pose );
		protocols::loops::Loops contig;
		contig.push_back(contigs[1]);
		template_chunks_.push_back(contig);
		template_contigs_.push_back(contig);
		TR << "Templates have been filtered so added first residue of pairing " << pairing << " as initial template " << template_poses_.size() << std::endl;
	}
	template_wts_.push_back( 0.0 ); // should not be used
	template_poses_.push_back( trimmed_pairing_pose );
	template_chunks_.push_back(contigs);
	template_contigs_.push_back(contigs);
	strand_pairings_template_indices_.insert( template_poses_.size() ); // keep track of which templates are pairings
	TR << "Added pairing " << pairing << " as template " << template_poses_.size() << std::endl;
}

void FoldTreeHybridize::superimpose_strand_pairings_to_templates(core::pose::Pose & pose) {
	if (!strand_pairings_template_indices_.size()) return;
	using namespace core::kinematics;
	kinematics::FoldTree const fold_tree = pose.fold_tree();
	// we need to superimpose starting from the rooted chunks which are either from templates or if none exist, the first strand pair chunks
	utility::vector1<std::string> atom_names;
	atom_names.push_back("CA");
	atom_names.push_back("N");
	atom_names.push_back("C");
	atom_names.push_back("O");
	std::set<core::Size>::iterator pairings_iter;

	// this loop should traverse the fold tree downstream starting from the root
	for ( kinematics::FoldTree::const_iterator it = fold_tree.begin(), it_end = fold_tree.end(); it != it_end; ++it ) {
		if (!it->is_jump() || it->start() == fold_tree.root()) continue;
		for (pairings_iter = strand_pairings_template_indices_.begin(); pairings_iter != strand_pairings_template_indices_.end(); ++pairings_iter) {
			core::Size resi_pdb = template_poses_[*pairings_iter]->pdb_info()->number(1);
			core::Size resj_pdb = template_poses_[*pairings_iter]->pdb_info()->number(2);
			// the pairing template pose index that represents this jump
			if (	(resi_pdb == (core::Size)it->start() && resj_pdb == (core::Size)it->stop()) ||
						(resj_pdb == (core::Size)it->start() && resi_pdb == (core::Size)it->stop())) {
				core::id::AtomID_Map< id::AtomID > atom_map;
				core::pose::initialize_atomid_map( atom_map, *template_poses_[*pairings_iter], core::id::BOGUS_ATOM_ID );
				// try to align to the initial template
				if ( !templates_with_incorrect_strand_pairings_.count(initial_template_index_) && *pairings_iter != initial_template_index_ ) {
					core::Size template_resi = map_pdb_info_number( *template_poses_[initial_template_index_], resi_pdb );
					core::Size template_resj = map_pdb_info_number( *template_poses_[initial_template_index_], resj_pdb );
					if (template_resi && template_resj) {
						// superimpose all atoms
						for (core::Size i = 1; i<=atom_names.size();++i) {
							core::id::AtomID const id1i( template_poses_[*pairings_iter]->residue(1).atom_index(atom_names[i]), 1 );
							core::id::AtomID const id2i( template_poses_[initial_template_index_]->residue(template_resi).atom_index(atom_names[i]), template_resi );
							atom_map[ id1i ] = id2i;
							core::id::AtomID const id1j( template_poses_[*pairings_iter]->residue(2).atom_index(atom_names[i]), 2 );
							core::id::AtomID const id2j( template_poses_[initial_template_index_]->residue(template_resj).atom_index(atom_names[i]), template_resj );
							atom_map[ id1j ] = id2j;
						}
						TR << "Superimpose strand pair " << *pairings_iter << " (" << resi_pdb << " " << resj_pdb << ") at " << resi_pdb << " and " << resj_pdb << " to initial template " << initial_template_index_ << std::endl;
					} else if (template_resi) {
						// superimpose i strand atoms
						for (core::Size i = 1; i<=atom_names.size();++i) {
							core::id::AtomID const id1i( template_poses_[*pairings_iter]->residue(1).atom_index(atom_names[i]), 1 );
							core::id::AtomID const id2i( template_poses_[initial_template_index_]->residue(template_resi).atom_index(atom_names[i]), template_resi );
							atom_map[ id1i ] = id2i;
						}
						TR << "Superimpose strand pair " << *pairings_iter << " at " << resi_pdb << " to initial template " << initial_template_index_ << std::endl;
					} else if (template_resj) {
						// superimpose j strand atoms
						for (core::Size i = 1; i<=atom_names.size();++i) {
							core::id::AtomID const id1j( template_poses_[*pairings_iter]->residue(2).atom_index(atom_names[i]), 2 );
							core::id::AtomID const id2j( template_poses_[initial_template_index_]->residue(template_resj).atom_index(atom_names[i]), template_resj );
							atom_map[ id1j ] = id2j;
						}
						TR << "Superimpose strand pair " << *pairings_iter << " at " << resj_pdb << " to initial template " << initial_template_index_ << std::endl;
					}
					if (template_resi || template_resj) {
						core::Real rms = core::scoring::superimpose_pose( *template_poses_[*pairings_iter], *template_poses_[initial_template_index_], atom_map );
						TR << "rms: " << ObjexxFCL::format::F(8,3,rms) << std::endl;
						continue;
					}
				}
				// if we reach this point, try to align to upstream pairs
				bool do_continue = false;
				for ( kinematics::FoldTree::const_iterator iti = fold_tree.begin(), iti_end = fold_tree.end(); iti != iti_end; ++iti ) {
					if (*it == *iti || !iti->is_jump() || iti->start() == fold_tree.root() || iti->stop() != it->start()) continue;
					std::set<core::Size>::iterator pairings_iteri;
					for (pairings_iteri = strand_pairings_template_indices_.begin(); pairings_iteri != strand_pairings_template_indices_.end(); ++pairings_iteri) {
						core::Size resii_pdb = template_poses_[*pairings_iteri]->pdb_info()->number(1);
						core::Size resji_pdb = template_poses_[*pairings_iteri]->pdb_info()->number(2);
						if ((resii_pdb == (core::Size)iti->start() && resji_pdb == (core::Size)iti->stop()) ||
								(resji_pdb == (core::Size)iti->start() && resii_pdb == (core::Size)iti->stop())) {
							core::Size template_res = map_pdb_info_number( *template_poses_[*pairings_iteri], iti->stop() );
							if (template_res) {
								core::Size pair_res = map_pdb_info_number( *template_poses_[*pairings_iter], it->start() );
								// superimpose strand atoms
								for (core::Size j = 1; j<=atom_names.size();++j) {
									core::id::AtomID const id1i( template_poses_[*pairings_iter]->residue(pair_res).atom_index(atom_names[j]), pair_res );
									core::id::AtomID const id2i( template_poses_[*pairings_iteri]->residue(template_res).atom_index(atom_names[j]), template_res );
									atom_map[ id1i ] = id2i;
								}
								TR << "Superimpose strand pair " << *pairings_iter << " at " << it->start() << " to pair " << *pairings_iteri << std::endl;
								core::Real rms = core::scoring::superimpose_pose( *template_poses_[*pairings_iter], *template_poses_[*pairings_iteri], atom_map );
								TR << "rms: " << ObjexxFCL::format::F(8,3,rms) << std::endl;
								do_continue = true;
							}
						}
					}
				}
				if (do_continue) continue;
				// if we reach this point, try to align to any non-strand pair template
				utility::vector1< core::Size > candidate_templates;
				for (core::Size i=1; i<=template_poses_.size(); ++i) {
					if (  i == initial_template_index_ || templates_with_incorrect_strand_pairings_.count(i) || strand_pairings_template_indices_.count(i)) continue;
					if ( map_pdb_info_number( *template_poses_[i], resi_pdb ) || map_pdb_info_number( *template_poses_[i], resj_pdb ) )
						candidate_templates.push_back(i);
				}
				if (candidate_templates.size()) {
					utility::vector1< core::Size >::iterator it;
					std::random_shuffle ( candidate_templates.begin(), candidate_templates.end() );
					for (it=candidate_templates.begin(); it!=candidate_templates.end(); ++it) {
						core::Size template_resi = map_pdb_info_number( *template_poses_[*it], resi_pdb );
						core::Size template_resj = map_pdb_info_number( *template_poses_[*it], resj_pdb );
						if (template_resi && template_resj) {
							// superimpose all atoms
							for (core::Size i = 1; i<=atom_names.size();++i) {
								core::id::AtomID const id1i( template_poses_[*pairings_iter]->residue(1).atom_index(atom_names[i]), 1 );
								core::id::AtomID const id2i( template_poses_[*it]->residue(template_resi).atom_index(atom_names[i]), template_resi );
								atom_map[ id1i ] = id2i;
								core::id::AtomID const id1j( template_poses_[*pairings_iter]->residue(2).atom_index(atom_names[i]), 2 );
								core::id::AtomID const id2j( template_poses_[*it]->residue(template_resj).atom_index(atom_names[i]), template_resj );
								atom_map[ id1j ] = id2j;
							}
						} else if (template_resi) {
							// superimpose i strand atoms
							for (core::Size i = 1; i<=atom_names.size();++i) {
								core::id::AtomID const id1i( template_poses_[*pairings_iter]->residue(1).atom_index(atom_names[i]), 1 );
								core::id::AtomID const id2i( template_poses_[*it]->residue(template_resi).atom_index(atom_names[i]), template_resi );
								atom_map[ id1i ] = id2i;
							}
						} else if (template_resj) {
							// superimpose j strand atoms
							for (core::Size i = 1; i<=atom_names.size();++i) {
								core::id::AtomID const id1j( template_poses_[*pairings_iter]->residue(2).atom_index(atom_names[i]), 2 );
								core::id::AtomID const id2j( template_poses_[*it]->residue(template_resj).atom_index(atom_names[i]), template_resj );
								atom_map[ id1j ] = id2j;
							}
						}
						if (template_resi || template_resj) {
							TR << "Superimpose strand pair " << *pairings_iter << " to random template " << *it << std::endl;
							core::Real rms = core::scoring::superimpose_pose( *template_poses_[*pairings_iter], *template_poses_[*it], atom_map );
							TR << "rms: " << ObjexxFCL::format::F(8,3,rms) << std::endl;
							do_continue = true;
							break;
						}
					}
				}
				if (do_continue) continue;
				TR << "Cannot superimpose strand pairing " << *pairings_iter << " to a template so treating it as a floating pair" << std::endl;
				floating_pairs_.insert(*pairings_iter);
			}
		}
	}
}

core::Size FoldTreeHybridize::map_pdb_info_number( const core::pose::Pose & pose, core::Size pdb_res ) {
	core::Size mapped = 0;
	for (core::Size i=1; i<=pose.total_residue(); ++i) {
		if (pose.pdb_info()->number(i) == (int)pdb_res) {
			mapped = i;
			break;
		}
	}
	return mapped;
}

void FoldTreeHybridize::filter_templates(std::set< core::Size > const & templates_to_remove) {
	if (templates_to_remove.size() == 0) return;

	utility::vector1 < core::pose::PoseOP > template_poses_filtered;
	utility::vector1 < core::Real > template_wts_filtered;
	utility::vector1 < protocols::loops::Loops > template_chunks_filtered;
	utility::vector1 < protocols::loops::Loops > template_contigs_filtered;
	for (core::Size i=1; i<=template_poses_.size(); ++i) {
		if (templates_to_remove.count(i)) {
			if (i == initial_template_index_) {
				initial_template_index_ = 1; // default to 1
				TR << "filter_templates: removing initial template: " << i << std::endl;
			} else {
				TR << "filter_templates: removing template: " << i << std::endl;
			}
			continue;
		}
		template_poses_filtered.push_back(template_poses_[i]);
		template_wts_filtered.push_back(template_wts_[i]);
		template_chunks_filtered.push_back(template_chunks_[i]);
		template_contigs_filtered.push_back(template_contigs_[i]);
	}
	if (!template_poses_filtered.size())
		TR << "Warning! all templates were removed." << std::endl;
	template_poses_ = template_poses_filtered;
	template_wts_ = template_wts_filtered;
	template_chunks_ = template_chunks_filtered;
	template_contigs_ = template_contigs_filtered;
	normalize_template_wts();
}

protocols::simple_moves::ClassicFragmentMoverOP FoldTreeHybridize::get_pairings_jump_mover() {
	assert( jump_frags_ );
  core::kinematics::MoveMapOP movemap = new core::kinematics::MoveMap;
  movemap->set_bb( true );
  movemap->set_jump( true );
  protocols::simple_moves::ClassicFragmentMoverOP jump_mover = new protocols::simple_moves::ClassicFragmentMover( jump_frags_, movemap );
  jump_mover->type( "JumpMoves" );
  jump_mover->set_check_ss( false ); // this doesn't make sense with jump fragments
  jump_mover->enable_end_bias_check( false ); //no sense for discontinuous fragments
	return jump_mover;
}

std::set< core::Size > FoldTreeHybridize::get_pairings_residues() {
	std::set<core::Size> strand_pair_residues;
	for (core::Size i = 1; i<=strand_pairs_.size();++i) {
		strand_pair_residues.insert(strand_pairs_[i].first);
		strand_pair_residues.insert(strand_pairs_[i].second);
	}
	return strand_pair_residues;
}


// end of strand pairings methods

// convergence checker from ClassicAbinitio
/// @brief (helper) functor class which keeps track of old pose for the
/// convergence check in stage3 cycles
/// @detail
/// Similar to Classic Abinitio's convergence check but checks moves
/// with big fragments for 3 angstrom rmsd convergence and for small
/// fragments, 1.5 angstrom rmsd convergence. It checks after 200 cycles
/// compared to 100 used in ClassicAbinitio
class hConvergenceCheck;
typedef  utility::pointer::owning_ptr< hConvergenceCheck >  hConvergenceCheckOP;

class hConvergenceCheck : public moves::PoseCondition {
public:
  hConvergenceCheck() : bInit_( false ), ct_( 0 ) {}
  void reset() { ct_ = 0; bInit_ = false; residue_selection_big_frags_.clear(); residue_selection_small_frags_.clear(); }
  void set_trials( moves::TrialMoverOP trin ) {
    trials_ = trin;
    runtime_assert( trials_->keep_stats_type() < moves::no_stats );
    last_move_ = 0;
  }
	void set_residue_selection_big_frags( std::list< core::Size > const & residue_selection ) {
		residue_selection_big_frags_ = residue_selection;
	}
	void set_residue_selection_small_frags( std::list< core::Size > const & residue_selection ) {
		residue_selection_small_frags_ = residue_selection;
	}
  virtual bool operator() ( const core::pose::Pose & pose );
private:
  core::pose::Pose very_old_pose_;
  bool bInit_;
  core::Size ct_;
	std::list< core::Size > residue_selection_big_frags_;
	std::list< core::Size > residue_selection_small_frags_;
  moves::TrialMoverOP trials_;
  core::Size last_move_;
};

// keep going --> return true
bool hConvergenceCheck::operator() ( const core::pose::Pose & pose ) {
  if ( !bInit_ ) {
    bInit_ = true;
		TR.Trace << "hConvergenceCheck residue_selection_small_frags size: " << residue_selection_small_frags_.size() << std::endl;
		TR.Trace << "hConvergenceCheck residue_selection_big_frags size: " << residue_selection_big_frags_.size() << std::endl;
    very_old_pose_ = pose;
    return true;
  }
  runtime_assert( trials_ );
  TR.Trace << "TrialCounter in hConvergenceCheck: " << trials_->num_accepts() << std::endl;
  if ( numeric::mod(trials_->num_accepts(),200) != 0 ) return true;
  if ( (Size) trials_->num_accepts() <= last_move_ ) return true;
  last_move_ = trials_->num_accepts();
  core::Real converge_rms_small = (residue_selection_small_frags_.size()) ?
		core::scoring::CA_rmsd( very_old_pose_, pose, residue_selection_small_frags_ ) : 0.0;
  core::Real converge_rms_big = (residue_selection_big_frags_.size()) ?
		core::scoring::CA_rmsd( very_old_pose_, pose, residue_selection_big_frags_ ) : 0.0;
  very_old_pose_ = pose;
  if ( converge_rms_big >= 3.0 || converge_rms_small >= 1.5 || (!converge_rms_small && !converge_rms_big)) {
		TR.Trace << "hConvergenceCheck continue, converge_rms_big: " << converge_rms_big << " converge_rms_small: " << converge_rms_small <<  std::endl;
    return true;
  }
  // if we get here thing is converged stop the While-Loop
  TR.Info << "stop cycles due to convergence, converge_rms_big: " << converge_rms_big << " converge_rms_small: " << converge_rms_small <<  std::endl;
  return false;
}



void
FoldTreeHybridize::apply(core::pose::Pose & pose) {
	if (hybridize_setup_) {
		setup_for_parser();
	} else {
		core::Size nres_nonvirt = get_num_residues_nonvirt(pose);
		residue_sample_template_.resize(nres_nonvirt, true);
		residue_sample_abinitio_.resize(nres_nonvirt, true);
		residue_max_registry_shift_.resize(nres_nonvirt, 0);
	}

	// save target sequence
	target_sequence_ = pose.sequence();

	// strand pairings - include chunks from pairings
	// this will add pairings templates and may remove templates that are missing strand pairings if filter_templates option is used
	add_strand_pairings();

	setup_foldtree(pose); // easier said than done!

	// strand pairings - superimpose pairings to templates to place them in the same coordinate frame
  superimpose_strand_pairings_to_templates(pose);

	bool has_strand_pairings = (strand_pairings_template_indices_.size()) ? true : false;

	// setup constraints
	//  note: ignore pairings residues (strand pairings templates) for auto generated constraints
	if ( scorefxn_->get_weight( core::scoring::atom_pair_constraint ) != 0 ) {
		setup_centroid_constraints( pose, template_poses_, template_wts_, cst_file_, get_pairings_residues() );
		if (add_hetatm_) {
			add_non_protein_cst(pose, hetatm_self_cst_weight_, hetatm_prot_cst_weight_);
		}
	}

	// Initialize the structure
	bool use_random_template = false;
	if (initialize_pose_by_templates_) {
		ChunkTrialMover initialize_chunk_mover(template_poses_, template_chunks_, ss_chunks_pose_, use_random_template, all_chunks, max_registry_shift_);
		initialize_chunk_mover.set_template(initial_template_index_);
		initialize_chunk_mover.apply(pose);

		// strand pairings
		if (has_strand_pairings) {
			// apply strand pairing jumps to place floating pairs
			protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();
			jump_mover->apply_at_all_positions( pose );
			// insert strand pairing template chunks (to place template pairs)
			for (std::set<core::Size>::iterator pairings_iter = strand_pairings_template_indices_.begin(); pairings_iter != strand_pairings_template_indices_.end(); ++pairings_iter) {
				if (floating_pairs_.count(*pairings_iter)) continue;
				initialize_chunk_mover.set_template(*pairings_iter);
				initialize_chunk_mover.apply(pose);
			}
		}
	}

	// ab initio ramping up of weights
	// set up scorefunctions
	core::scoring::ScoreFunctionOP score0=scorefxn_->clone(),
                                     score1=scorefxn_->clone(),
                                     score2=scorefxn_->clone(),
                                     score5=scorefxn_->clone(),
                                     score3=scorefxn_->clone();
	setup_scorefunctions( score0, score1, score2, score5, score3 );

	if ( !core::pose::symmetry::is_symmetric(pose) )
		translate_virt_to_CoM(pose);

	// coord csts _must_ be after the CoM gets moved
	if ( scorefxn_->get_weight( core::scoring::coordinate_constraint ) != 0) {
		if ( user_csts_.size() > 0 ) {
			setup_user_coordinate_constraints(pose,user_csts_);
		}
	}

	use_random_template = true;
	//Size max_registry_shift = max_registry_shift_;
	ChunkTrialMoverOP random_sample_chunk_mover(
		new ChunkTrialMover(template_poses_, template_chunks_, ss_chunks_pose_, use_random_template, random_chunk, residue_sample_template_,  residue_max_registry_shift_) );
	//random_sample_chunk_mover->set_movable_region(allowed_to_move_);

	// ignore strand pair templates, they will be sampled by a jump mover
	random_sample_chunk_mover->set_templates_to_ignore(strand_pairings_template_indices_);

	utility::vector1< core::Real > residue_weights_1mer_frags( get_residue_weights_for_1mers(pose) );
	utility::vector1< core::Real > residue_weights_small_frags( get_residue_weights_for_small_frags(pose) );
	utility::vector1< core::Real > residue_weights( get_residue_weights_for_big_frags(pose) );

	utility::vector1< core::Size > jump_anchors = get_jump_anchors();

	// set montecarlo temp
	core::Real temp = 2.0;

	// set up movers
	// Stage 1-3: chunks + big + small (at positions skipped by big) + 1mer (at positions skipped by small) frags + strand pair jumps if given
	RandomMoverOP random_chunk_and_frag_mover( new RandomMover() );
	// Stage 4: chunks + small + 1mer (at positions skipped by small) frags + strand pair jumps if given
	RandomMoverOP random_chunk_and_small_frag_mover( new RandomMover() );
	// Stage 4 smooth moves: chunks + small + 1mer (at positions skipped by small) frags + strand pair jumps if given
	RandomMoverOP random_chunk_and_small_frag_smooth_mover( new RandomMover() );

	WeightedFragmentTrialMoverOP frag_1mer_mover( new WeightedFragmentTrialMover( frag_libs_1mer_, residue_weights_1mer_frags, jump_anchors, top_n_big_frag_) );
	WeightedFragmentTrialMoverOP small_frag_gaps_mover( new WeightedFragmentTrialMover( frag_libs_small_, residue_weights_small_frags, jump_anchors, top_n_big_frag_) );
	WeightedFragmentTrialMoverOP top_big_frag_mover( new WeightedFragmentTrialMover( frag_libs_big_, residue_weights, jump_anchors, top_n_big_frag_) );
	WeightedFragmentTrialMoverOP small_frag_mover( new WeightedFragmentTrialMover( frag_libs_small_, residue_weights, jump_anchors, 0) );
	WeightedFragmentSmoothTrialMoverOP small_frag_smooth_mover( new WeightedFragmentSmoothTrialMover( frag_libs_small_, residue_weights, jump_anchors, 0, new GunnCost) );

	// automatically re-weight the fragment insertions if desired
	auto_frag_insertion_weight(frag_1mer_mover, small_frag_gaps_mover, top_big_frag_mover);

	core::Real total_frag_insertion_weight = small_frag_insertion_weight_+big_frag_insertion_weight_+frag_1mer_insertion_weight_;
	if ( total_frag_insertion_weight < 1. ) {
		random_chunk_and_frag_mover->add_mover(random_sample_chunk_mover, 1.-total_frag_insertion_weight);
		random_chunk_and_small_frag_mover->add_mover(random_sample_chunk_mover, 1.-total_frag_insertion_weight);
		random_chunk_and_small_frag_smooth_mover->add_mover(random_sample_chunk_mover, 1.-total_frag_insertion_weight);
	}
	bool do_frag_inserts = false;
	if ( total_frag_insertion_weight > 0. ) {
		core::Real sum_residue_weight = 0.;
		for ( Size ires=1; ires<= residue_weights.size(); ++ires ) sum_residue_weight += residue_weights[ires];
		if (sum_residue_weight > 1e-6) do_frag_inserts = true;
	}
	if (do_frag_inserts) {
		// use similar default fraction of fragment to jump moves as in KinematicAbinitio.cc
		core::Real jump_move_fraction = 1.0/(core::Real)(option[ jumps::invrate_jump_move ]+1.);

		// 1mers fragment insertions where small and big fragments are not allowed (small gaps between small chunks)
		if (frag_1mer_insertion_weight_ > 0.) {
			if (has_strand_pairings) {
				core::Real jump_insertion_weight = frag_1mer_insertion_weight_*jump_move_fraction;
				random_chunk_and_frag_mover->add_mover(frag_1mer_mover, frag_1mer_insertion_weight_-jump_insertion_weight);
				// strand pair jump insertions
				protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();
				random_chunk_and_frag_mover->add_mover(jump_mover, jump_insertion_weight);
			} else {
				random_chunk_and_frag_mover->add_mover(frag_1mer_mover, frag_1mer_insertion_weight_);
			}
		}

		// small fragment insertions where big fragments are not allowed (where big fragments cover jump_anchors)
		if (small_frag_insertion_weight_ > 0.) {
			if (has_strand_pairings) {
				core::Real jump_insertion_weight = small_frag_insertion_weight_*jump_move_fraction;
				random_chunk_and_frag_mover->add_mover(small_frag_gaps_mover, small_frag_insertion_weight_-jump_insertion_weight);
				// strand pair jump insertions
				protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();
				random_chunk_and_frag_mover->add_mover(jump_mover, jump_insertion_weight);
			} else {
				random_chunk_and_frag_mover->add_mover(small_frag_gaps_mover, small_frag_insertion_weight_);
			}
		}

		// big fragment and strand pairing jump insertions
		if (big_frag_insertion_weight_ > 0.) {
			if (has_strand_pairings) {
				core::Real jump_insertion_weight = big_frag_insertion_weight_*jump_move_fraction;
				random_chunk_and_frag_mover->add_mover(top_big_frag_mover, big_frag_insertion_weight_-jump_insertion_weight);
				// strand pair jump insertions
				protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();
				random_chunk_and_frag_mover->add_mover(jump_mover, jump_insertion_weight);
			} else {
				random_chunk_and_frag_mover->add_mover(top_big_frag_mover, big_frag_insertion_weight_);
			}
		}

		// small fragment insertions for stage 4
		if (small_frag_insertion_weight_+big_frag_insertion_weight_ > 0.) {
			if (has_strand_pairings) {
				core::Real jump_insertion_weight = (small_frag_insertion_weight_+big_frag_insertion_weight_)*jump_move_fraction;
				random_chunk_and_small_frag_mover->add_mover(small_frag_mover, small_frag_insertion_weight_+big_frag_insertion_weight_-jump_insertion_weight);
				random_chunk_and_small_frag_smooth_mover->add_mover(small_frag_smooth_mover, small_frag_insertion_weight_+big_frag_insertion_weight_-jump_insertion_weight);
				// strand pair jump insertions
				protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();
				random_chunk_and_small_frag_mover->add_mover(jump_mover, jump_insertion_weight);
				random_chunk_and_small_frag_smooth_mover->add_mover(jump_mover, jump_insertion_weight);
			} else {
				random_chunk_and_small_frag_mover->add_mover(small_frag_mover, small_frag_insertion_weight_+big_frag_insertion_weight_);
				random_chunk_and_small_frag_smooth_mover->add_mover(small_frag_smooth_mover, small_frag_insertion_weight_+big_frag_insertion_weight_);
			}
		}

		// 1mer fragment insertions for stage 4
		if (frag_1mer_insertion_weight_ > 0.) {
			if (has_strand_pairings) {
				core::Real jump_insertion_weight = frag_1mer_insertion_weight_*jump_move_fraction;
				random_chunk_and_small_frag_mover->add_mover(frag_1mer_mover, frag_1mer_insertion_weight_-jump_insertion_weight);
				random_chunk_and_small_frag_smooth_mover->add_mover(frag_1mer_mover, frag_1mer_insertion_weight_-jump_insertion_weight);
				// strand pair jump insertions
				protocols::simple_moves::ClassicFragmentMoverOP jump_mover = get_pairings_jump_mover();
				random_chunk_and_small_frag_mover->add_mover(jump_mover, jump_insertion_weight);
				random_chunk_and_small_frag_smooth_mover->add_mover(jump_mover, jump_insertion_weight);
			} else {
				random_chunk_and_small_frag_mover->add_mover(frag_1mer_mover, frag_1mer_insertion_weight_);
				random_chunk_and_small_frag_smooth_mover->add_mover(frag_1mer_mover, frag_1mer_insertion_weight_);
			}
		}
	}

	// determine the number of cycles
	core::Size stage1_max_cycles = (core::Size)((core::Real)stage1_1_cycles_*increase_cycles_);
	core::Size stage2_max_cycles = (core::Size)((core::Real)stage1_2_cycles_*increase_cycles_);
	core::Size stage3_max_cycles = (core::Size)((core::Real)stage1_3_cycles_*increase_cycles_);
	core::Size stage4_max_cycles = (core::Size)((core::Real)stage1_4_cycles_*increase_cycles_);

	// For stages 1-4 use the same ramping of chainbreaks as in KinematicAbinitio without close_chbrk_
	//  may want to look into using the constraints/jump sequence separation logic as in KinematicAbinitio

	// stage 1
	//  fragment moves to get rid of extended chains (should we include chunk moves?)
	//       this version: up to 2000 cycles until torsions are replaced
	//     casp10 version: 2000 cycles
	if (do_frag_inserts) {
    TR.Info <<  "\n===================================================================\n";
    TR.Info <<  "   Stage 1                                                         \n";
    TR.Info <<  "   Folding with score0 for max of " << stage1_max_cycles << std::endl;
		using namespace ObjexxFCL::format;
		AllResiduesChanged done( pose, residue_weights, jump_anchors );
		protocols::moves::MonteCarloOP mc1 = new protocols::moves::MonteCarlo( pose, *score0, temp );
		mc1->set_autotemp( false, temp );
		(*score0)(pose);
		bool all_res_changed = false;
		for (core::Size i=1; i<=stage1_max_cycles; ++i) {
			random_chunk_and_frag_mover->apply(pose);
			(*score0)(pose);
			mc1->boltzmann(pose, "RandomMover");
			if ( done(pose) ) {
				TR.Info << "Stage 1: Replaced extended chains after " << i << " cycles." << std::endl;
				all_res_changed = true;
				break;
			}
		}
		if (!all_res_changed) {
			TR.Warning << "Stage 1: Warning: extended chain may still remain after " << stage1_max_cycles << " cycles!" << std::endl;
			done.show_unmoved( pose, TR.Warning );
		}
		mc1->show_scores();
		mc1->show_counters();
		mc1->recover_low(pose);
		mc1->reset( pose );
	}

	// evaluation
	core::sequence::SequenceAlignmentOP native_aln;
	core::Real gdtmm = 0.0;
	if (native_ && native_->total_residue()) {
		gdtmm = get_gdtmm(*native_, pose, native_aln);
		core::pose::setPoseExtraScore( pose, "GDTMM_after_stage1_1", gdtmm);
		TR << "GDTMM_after_stage1_1" << ObjexxFCL::format::F(8,3,gdtmm) << std::endl;
	}

	// stage 2
	//  fragment and chunk insertion moves with score1
	//      this version: 2000 cycles with autotemp
	//    casp10 version: 2000 cycles
	{
    TR.Info <<  "\n===================================================================\n";
    TR.Info <<  "   Stage 2                                                         \n";
    TR.Info <<  "   Folding with score1 for " << stage2_max_cycles << std::endl;

		// ramp chainbreak weight as in KinematicAbinitio
		Real const setting( 0.25 / 3 * option[ jumps::increase_chainbreak ] );
		TR.Debug << scoring::name_from_score_type(scoring::linear_chainbreak) << " " << setting << std::endl;
		score1->set_weight(scoring::linear_chainbreak, setting);

		protocols::moves::MonteCarloOP mc2 = new protocols::moves::MonteCarlo( pose, *score1, temp );
		mc2->set_autotemp( true, temp );
		mc2->set_temperature( temp ); // temperature might have changed due to autotemp..
		mc2->reset( pose );
		(*score1)(pose);
		moves::TrialMoverOP stage2_trials = new moves::TrialMover( random_chunk_and_frag_mover, mc2 );
		stage2_trials->keep_stats_type( moves::accept_reject );
		moves::RepeatMover( stage2_trials, stage2_max_cycles ).apply( pose );

		mc2->show_scores();
		mc2->show_counters();
		mc2->recover_low(pose);
		mc2->reset( pose );
	}

	// evaluation
	if (native_ && native_->total_residue()) {
		gdtmm = get_gdtmm(*native_, pose, native_aln);
		core::pose::setPoseExtraScore( pose, "GDTMM_after_stage1_2", gdtmm);
		TR << "GDTMM_after_stage1_2" << ObjexxFCL::format::F(8,3,gdtmm) << std::endl;
	}

	// stage 3
	//  fragment and chunk insertion moves with alternating score 2 and 5
	//  (ClassicAbinitio uses 2000 cycles for each inner iteration)
	//      this version: up to 2000 cycles with autotemp, convergence checking, and ramped chainbreak score
	//    casp10 version: 200 cycles
	{
    TR.Info <<  "\n===================================================================\n";
    TR.Info <<  "   Stage 3                                                         \n";
    TR.Info <<  "   Folding with score2 and score5 for " << stage3_max_cycles <<std::endl;
		hConvergenceCheckOP convergence_checker ( NULL );
		if ( !option[ abinitio::skip_convergence_check ] ) {
			convergence_checker = new hConvergenceCheck;
			std::list< core::Size > residue_selection_big_frags;
			std::list< core::Size > residue_selection_small_frags;
			for (core::Size i = 1; i<=residue_weights.size(); ++i) {
				if (	residue_weights[i] > 0.0 &&
							pose.residue(i).is_protein() &&
							pose.residue(i).has("CA") ) residue_selection_big_frags.push_back(i);
			}
			for (core::Size i = 1; i<=residue_weights_small_frags.size(); ++i) {
				if (  residue_weights_small_frags[i] > 0.0 &&
							pose.residue(i).is_protein() &&
							pose.residue(i).has("CA") ) residue_selection_small_frags.push_back(i);
			}
			convergence_checker->set_residue_selection_big_frags(residue_selection_big_frags);
			convergence_checker->set_residue_selection_small_frags(residue_selection_small_frags);
		}

		for (int nmacro=1; nmacro<=10; ++nmacro) {
			// ramp chainbreak weight as in KinematicAbinitio
			Real chbrk_weight_stage_3a = 0;
			Real chbrk_weight_stage_3b = 0;
			Real progress( 1.0* nmacro/10 );
			Real const fact(  progress * 1.0/3  * option[ jumps::increase_chainbreak ]);
			chbrk_weight_stage_3a = 2.5 * fact;
			chbrk_weight_stage_3b = 0.5 * fact;
			TR.Debug << scoring::name_from_score_type(scoring::linear_chainbreak) << " " << chbrk_weight_stage_3a << std::endl;
			score2->set_weight(scoring::linear_chainbreak, chbrk_weight_stage_3a);
			TR.Debug << scoring::name_from_score_type(scoring::linear_chainbreak) << " " << chbrk_weight_stage_3b << std::endl;
			score5->set_weight(scoring::linear_chainbreak, chbrk_weight_stage_3b);

			protocols::moves::MonteCarloOP mc3 = new protocols::moves::MonteCarlo( pose, *score2, temp );
			moves::TrialMoverOP stage3_trials = new moves::TrialMover( random_chunk_and_frag_mover, mc3 );
			stage3_trials->keep_stats_type( moves::accept_reject );

			if ( numeric::mod( nmacro, 2 ) != 0 || nmacro > 6 ) {
				TR.Info << "Stage 3 select score2..." << std::endl;
				mc3->recover_low(pose);
				mc3->score_function( *score2 );
				mc3->set_autotemp( true, temp );
				mc3->set_temperature( temp );
				mc3->reset( pose );
				(*score2)( pose );
			} else {
				TR.Info << "Stage 3 select score5..." << std::endl;
				mc3->recover_low(pose);
				mc3->score_function( *score5 );
				mc3->set_autotemp( true, temp );
				mc3->set_temperature( temp );
				mc3->reset( pose );
				(*score5)( pose );
			}

			TR.Info << "Stage 3 loop iteration " << nmacro << std::endl;
			if ( convergence_checker ) {
				convergence_checker->set_trials( stage3_trials );
				moves::WhileMover( stage3_trials, stage3_max_cycles, convergence_checker ).apply( pose );
			} else {
				moves::RepeatMover( stage3_trials, stage3_max_cycles ).apply( pose );
			}
			TR.Debug << "finished" << std::endl;
			mc3->show_scores();
			mc3->show_counters();
			mc3->recover_low(pose);
			mc3->reset( pose );
		}
	}

	// evaluation
	if (native_ && native_->total_residue()) {
		gdtmm = get_gdtmm(*native_, pose, native_aln);
		core::pose::setPoseExtraScore( pose, "GDTMM_after_stage1_3", gdtmm);
		TR << "GDTMM_after_stage1_3" << ObjexxFCL::format::F(8,3,gdtmm) << std::endl;
	}

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	if (!option[cm::hybridize::stage1_4_cenrot_score].user())
	{
		// stage 4 -- ramp up chainbreak
		//    this version: 3 steps, 4000 cycles (only small fragments and last 2 steps smooth moves)
		//  casp10 version: 4 steps, 500 cycles
		TR.Info <<  "\n===================================================================\n";
		TR.Info <<  "   Stage 4                                                         \n";
		TR.Info <<  "   Folding with score3 for " << stage4_max_cycles <<std::endl;
		for (int nmacro=1; nmacro<=3; ++nmacro) {

			// ramp chainbreak weight as in KinematicAbinitio
			Real progress( 1.0* nmacro/3 );
			Real const setting( ( 1.5*progress+2.5 ) * ( 1.0/3) * option[ jumps::increase_chainbreak ]);
			TR.Debug << scoring::name_from_score_type(scoring::linear_chainbreak) << " " << setting << std::endl;
			score3->set_weight( core::scoring::linear_chainbreak, setting );
			if (overlap_chainbreaks_) {
				TR.Debug << scoring::name_from_score_type(scoring::overlap_chainbreak) << " " << progress << std::endl;
				score3->set_weight( core::scoring::overlap_chainbreak, progress );
			}

			protocols::moves::MonteCarloOP mc4 = new protocols::moves::MonteCarlo( pose, *score3, temp );
			mc4->set_autotemp( true, temp );
			mc4->set_temperature( temp );
			mc4->reset( pose );
			(*score3)( pose );
			moves::TrialMoverOP stage4_trials;
			if ( nmacro == 1 ) {
				TR.Info << "Stage 4 loop iteration " << nmacro << ": small fragments trials" << std::endl;
				stage4_trials = new moves::TrialMover( random_chunk_and_small_frag_mover, mc4 );
			} else {
				TR.Info << "Stage 4 loop iteration " << nmacro << ": small fragments smooth trials" << std::endl;
				stage4_trials = new moves::TrialMover(random_chunk_and_small_frag_smooth_mover,  mc4);
			}
			moves::RepeatMover( stage4_trials, stage4_max_cycles ).apply(pose);
			TR.Debug << "finished" << std::endl;
			mc4->show_scores();
			mc4->show_counters();
			mc4->recover_low(pose);
			mc4->reset( pose );
		}
	}
	else { // cenrot version stage4
		// switch to cenrot model
		protocols::moves::MoverOP tocenrot =
			new protocols::simple_moves::SwitchResidueTypeSetMover( core::chemical::CENTROID_ROT );
		tocenrot->apply( pose );
		// setup score
		core::scoring::ScoreFunctionOP score_cenrot =
			core::scoring::ScoreFunctionFactory::create_score_function( option[cm::hybridize::stage1_4_cenrot_score]() );
		// packer
		using namespace core::pack::task;
		simple_moves::PackRotamersMoverOP pack_rotamers;
		pack_rotamers = new protocols::simple_moves::PackRotamersMover();
		TaskFactoryOP main_task_factory = new TaskFactory;
		main_task_factory->push_back( new operation::RestrictToRepacking );
		pack_rotamers->task_factory(main_task_factory);
		pack_rotamers->score_function(score_cenrot);
		pack_rotamers->apply(pose);
		//setup mover
		moves::SequenceMoverOP combo_small( new moves::SequenceMover() );
		combo_small->add_mover(random_chunk_and_small_frag_mover);
		combo_small->add_mover(pack_rotamers);
		moves::SequenceMoverOP combo_smooth( new moves::SequenceMover() );
		combo_smooth->add_mover(random_chunk_and_small_frag_smooth_mover);
		combo_smooth->add_mover(pack_rotamers);

		TR.Info <<  "\n===================================================================\n";
		TR.Info <<  "   Stage 4                                                         \n";
		TR.Info <<  "   Folding with score_cenrot for " << stage4_max_cycles <<std::endl;
		for (int nmacro=1; nmacro<=3; ++nmacro) {

			// ramp chainbreak weight as in KinematicAbinitio
			Real progress( 1.0* nmacro/3 );
			Real const setting( ( 1.5*progress+2.5 ) * ( 1.0/3) * option[ jumps::increase_chainbreak ]);
			TR.Debug << scoring::name_from_score_type(scoring::linear_chainbreak) << " " << setting << std::endl;
			score_cenrot->set_weight( core::scoring::linear_chainbreak, setting );
			if (overlap_chainbreaks_) {
				TR.Debug << scoring::name_from_score_type(scoring::overlap_chainbreak) << " " << progress << std::endl;
				score_cenrot->set_weight( core::scoring::overlap_chainbreak, progress );
			}

			protocols::moves::MonteCarloOP mc4 = new protocols::moves::MonteCarlo( pose, *score_cenrot, temp );
			mc4->set_autotemp( true, temp );
			mc4->set_temperature( temp );
			mc4->reset( pose );
			(*score_cenrot)( pose );
			moves::TrialMoverOP stage4_trials;
			if ( nmacro == 1 ) {
				TR.Info << "Stage 4 loop iteration " << nmacro << ": small fragments trials" << std::endl;
				stage4_trials = new moves::TrialMover( combo_small, mc4 );
			} else {
				TR.Info << "Stage 4 loop iteration " << nmacro << ": small fragments smooth trials" << std::endl;
				stage4_trials = new moves::TrialMover( combo_smooth,  mc4);
			}
			moves::RepeatMover( stage4_trials, stage4_max_cycles ).apply(pose);
			TR.Debug << "finished" << std::endl;
			mc4->show_scores();
			mc4->show_counters();
			mc4->recover_low(pose);
			mc4->reset( pose );
		}
	}

	// evaluation
	if (native_ && native_->total_residue()) {
		gdtmm = get_gdtmm(*native_, pose, native_aln);
		core::pose::setPoseExtraScore( pose, "GDTMM_after_stage1_4", gdtmm);
		TR << "GDTMM_after_stage1_4" << ObjexxFCL::format::F(8,3,gdtmm) << std::endl;
	}

	pose.remove_constraints();
	restore_original_foldtree(pose);

	for (Size ires=1; ires<=pose.total_residue(); ++ires) {
		using namespace ObjexxFCL::format;
		TR.Debug << "Chunk trial counter:" << I(4,ires) << I(8, random_sample_chunk_mover->trial_counter(ires)) << std::endl;
	}
}

void FoldTreeHybridize::auto_frag_insertion_weight(
		WeightedFragmentTrialMoverOP & frag_1mer_trial_mover,
		WeightedFragmentTrialMoverOP & small_frag_trial_mover,
		WeightedFragmentTrialMoverOP & big_frag_trial_mover
) {
	using namespace ObjexxFCL::format;
	if (!auto_frag_insertion_weight_) return;
	core::Size frag_1mer_n_frags = frag_1mer_trial_mover->get_nr_frags();
	if (!frag_1mer_n_frags) frag_1mer_n_frags = top_n_small_frag_;
	core::Size small_n_frags = small_frag_trial_mover->get_nr_frags();
	if (!small_n_frags) small_n_frags = top_n_small_frag_;
	core::Size big_n_frags = big_frag_trial_mover->get_nr_frags();
	if (!big_n_frags) big_n_frags = top_n_big_frag_;

	core::Size template_pos_coverage = 0;
	for (core::Size i = 1; i<=template_chunks_.size(); ++i) {
		if (strand_pairings_template_indices_.count(i)) continue;
		template_pos_coverage += template_chunks_[i].num_loop();
	}
	template_pos_coverage *= chunk_insertion_weight_;

	core::Size frag_1mer_pos_coverage = frag_1mer_trial_mover->get_total_frames()*frag_1mer_n_frags;
	core::Size small_frag_pos_coverage = small_frag_trial_mover->get_total_frames()*small_n_frags;
	core::Size big_frag_pos_coverage = big_frag_trial_mover->get_total_frames()*big_n_frags;

	core::Real sum = (core::Real)(frag_1mer_pos_coverage+small_frag_pos_coverage+big_frag_pos_coverage+template_pos_coverage);

	frag_1mer_insertion_weight_ = ((core::Real) frag_1mer_pos_coverage)/sum;
	small_frag_insertion_weight_ = ((core::Real) small_frag_pos_coverage)/sum;
	big_frag_insertion_weight_ = ((core::Real) big_frag_pos_coverage)/sum;

	TR.Info << "auto_frag_insertion_weight for 1mer fragments: " << F(7,5,frag_1mer_insertion_weight_) << " " << frag_1mer_pos_coverage << std::endl;
	TR.Info << "auto_frag_insertion_weight for small fragments: " << F(7,5,small_frag_insertion_weight_) << " " << small_frag_pos_coverage << std::endl;
	TR.Info << "auto_frag_insertion_weight for big fragments: " << F(7,5,big_frag_insertion_weight_) << " " << big_frag_pos_coverage << std::endl;
	TR.Info << "auto_frag_insertion_weight for template chunks: " <<
		F(7,5,1.0-frag_1mer_insertion_weight_-big_frag_insertion_weight_-small_frag_insertion_weight_) << " " << template_pos_coverage << std::endl;
}

std::string FoldTreeHybridize::get_name() const
{
	return "FoldTreeHybridize";
}

protocols::moves::MoverOP FoldTreeHybridize::clone() const { return new FoldTreeHybridize( *this ); }
protocols::moves::MoverOP FoldTreeHybridize::fresh_instance() const { return new FoldTreeHybridize; }

void
FoldTreeHybridize::parse_my_tag(
								utility::tag::TagCOP tag,
								basic::datacache::DataMap & data,
								filters::Filters_map const &,
								moves::Movers_map const &,
								core::pose::Pose const & pose )
{
	if( tag->hasOption( "initialize_pose_by_templates" ) )
		initialize_pose_by_templates_ = tag->getOption< bool >( "initialize_pose_by_templates" );

	if (tag->hasOption( "realign_domains" )){
		realign_domains_ = tag->getOption< bool >( "realign_domains" );
	}

	if( tag->hasOption( "add_non_init_chunks" ) )
		add_non_init_chunks_ = tag->getOption< bool >( "add_non_init_chunks" );

	if( tag->hasOption( "increase_cycles" ) )
		set_increase_cycles( tag->getOption< core::Real >( "increase_cycles" ) );

	if( tag->hasOption( "stage1_cycles" ) )
		stage1_1_cycles_ = tag->getOption< core::Size >( "stage1_cycles" );
	if( tag->hasOption( "stage2_cycles" ) )
		stage1_2_cycles_ = tag->getOption< core::Size >( "stage2_cycles" );
	if( tag->hasOption( "stage3_cycles" ) )
		stage1_3_cycles_ = tag->getOption< core::Size >( "stage3_cycles" );
	if( tag->hasOption( "stage4_cycles" ) )
		stage1_4_cycles_ = tag->getOption< core::Size >( "stage4_cycles" );

	if( tag->hasOption( "auto_frag_insertion_weight" ) )
		auto_frag_insertion_weight_ = tag->getOption< bool >( "auto_frag_insertion_weight" );
	if( tag->hasOption( "frag_weight_aligned" ) )
		frag_weight_aligned_ = tag->getOption< core::Real >( "frag_weight_aligned" );
	if( tag->hasOption( "frag_1mer_insertion_weight" ) )
		frag_1mer_insertion_weight_ = tag->getOption< core::Real >( "frag_1mer_insertion_weight" );
	if( tag->hasOption( "small_frag_insertion_weight" ) )
		small_frag_insertion_weight_ = tag->getOption< core::Real >( "small_frag_insertion_weight" );
	if( tag->hasOption( "big_frag_insertion_weight" ) )
		big_frag_insertion_weight_ = tag->getOption< core::Real >( "big_frag_insertion_weight" );
	if( tag->hasOption( "max_registry_shift" ) )
		max_registry_shift_ = tag->getOption< core::Size >( "max_registry_shift" );


	if( tag->hasOption( "scorefxn" ) ) {
		std::string const scorefxn_name( tag->getOption<std::string>( "scorefxn" ) );
		set_scorefunction ( (data.get< core::scoring::ScoreFunction * >( "scorefxns", scorefxn_name ))->clone() );
	}

	if( tag->hasOption( "hybridize_setup" ) ) {
		std::string const setup_data_name( tag->getOption<std::string>( "hybridize_setup" ) );
		hybridize_setup_ = (data.get< protocols::hybridization::HybridizeSetup * >( "HybridizeSetup", setup_data_name ))->clone();
		TR << hybridize_setup_->nres_tgt_asu() << std::endl;
	}
	else {
		utility_exit_with_message("Fatal error: hybridize_setup needs to be defined!");
	}

	residue_sample_template_.resize(hybridize_setup_->nres_tgt_asu(), true);
	residue_sample_abinitio_.resize(hybridize_setup_->nres_tgt_asu(), true);
	residue_max_registry_shift_.resize(hybridize_setup_->nres_tgt_asu(), 0);

	utility::vector1< utility::tag::TagCOP > const branch_tags( tag->getTags() );
	utility::vector1< utility::tag::TagCOP >::const_iterator tag_it;
	for (tag_it = branch_tags.begin(); tag_it != branch_tags.end(); ++tag_it) {
		// per-residue control
		if ( (*tag_it)->getName() == "DetailedControls" ) {
				if( (*tag_it)->hasOption( "task_operations" ) ){
						core::pack::task::TaskFactoryOP task_factory = protocols::rosetta_scripts::parse_task_operations( *tag_it, data );
						core::pack::task::PackerTaskOP task = task_factory->create_task_and_apply_taskoperations( pose );

						for( core::Size ires = 1; ires <= hybridize_setup_->nres_tgt_asu(); ++ires ){

								if( task->residue_task( ires ).being_designed() && task->residue_task( ires ).being_packed() ) {
										// residue_cst_cross_chain_[ires] = true;
								}
								else {
										residue_sample_template_[ires] = false;
										residue_sample_abinitio_[ires] = false;
								}
						}
				}
				else {
						core::Size start_res = (*tag_it)->getOption<core::Size>( "start_res", 1 );
						core::Size stop_res = (*tag_it)->getOption<core::Size>( "stop_res", hybridize_setup_->nres_tgt_asu() );
						if( (*tag_it)->hasOption( "sample_template" ) ){
								bool sample_template = (*tag_it)->getOption<bool>( "sample_template", true );
								for (core::Size ires=start_res; ires<=stop_res; ++ires) {
										residue_sample_template_[ires] = sample_template;
								}
						}
						if( (*tag_it)->hasOption( "sample_abinitio" ) ){
								bool sample_abinitio = (*tag_it)->getOption<bool>( "sample_abinitio", true );
								for (core::Size ires=start_res; ires<=stop_res; ++ires) {
										residue_sample_abinitio_[ires] = sample_abinitio;
								}
						}
						if( (*tag_it)->hasOption( "max_registry_shift" ) ){
								core::Size max_registry_shift = (*tag_it)->getOption<core::Size>( "max_registry_shift", 0 );
								for (core::Size ires=start_res; ires<=stop_res; ++ires) {
										residue_max_registry_shift_[ires] = max_registry_shift; // restraints within the domain
								}
						}
				}
		}

		// strand pairings
		if ( (*tag_it)->getName() == "Pairings" ) {
			pairings_file_ = (*tag_it)->getOption< std::string >( "file", "" );
			if ( (*tag_it)->hasOption("sheets") ) {
				core::Size sheets = (*tag_it)->getOption< core::Size >( "sheets" );
				sheets_.clear();
				random_sheets_.clear();
				sheets_.push_back(sheets);
			} else if ( (*tag_it)->hasOption("random_sheets") ) {
				core::Size random_sheets = (*tag_it)->getOption< core::Size >( "random_sheets" );
				sheets_.clear();
				random_sheets_.clear();
				random_sheets_.push_back(random_sheets);
			}
			filter_templates_ = (*tag_it)->getOption<bool>( "filter_templates" , 0 );
		}
	}
}

/////////////
// creator
std::string
FoldTreeHybridizeCreator::keyname() const {
	return FoldTreeHybridizeCreator::mover_name();
}

protocols::moves::MoverOP
FoldTreeHybridizeCreator::create_mover() const {
	return new FoldTreeHybridize;
}

std::string
FoldTreeHybridizeCreator::mover_name() {
	return "FoldTreeHybridize";
}

} // hybridization
} // protocols

