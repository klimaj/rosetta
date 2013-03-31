// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/rna/RNA_Util.hh
/// @brief
/// @author Rhiju

#ifndef INCLUDED_core_scoring_rna_RNA_Util_hh
#define INCLUDED_core_scoring_rna_RNA_Util_hh

#include <core/types.hh>

#include <core/chemical/AA.hh>
#include <core/conformation/Residue.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <numeric/xyzMatrix.hh>
#include <numeric/xyzVector.hh>
#include <core/kinematics/Stub.hh>
#include <utility/vector1.hh>



namespace core {
namespace scoring {
namespace rna{

///////////////////////////////////////////////////////////////////////////////
enum __RNA_UTIL__ {X, WATSON_CRICK, HOOGSTEEN, SUGAR, O2STAR, PHOSPHATE };
Size const NUM_EDGES( 3 );
Size const NUM_RNA_TORSIONS( 10 );
Size const NUM_RNA_MAINCHAIN_TORSIONS( 6 );
Size const NUM_RNA_CHI_TORSIONS( NUM_RNA_TORSIONS - NUM_RNA_MAINCHAIN_TORSIONS  );

///////////////////////////////////////////////////////////////////////////////
extern utility::vector1< std::string > non_main_chain_sugar_atoms;
void	initialize_non_main_chain_sugar_atoms();

///////////////////////////////////////////////////////////////////////////////
Size
convert_acgu_to_1234( char const c );

char get_edge_from_num( Size const num );

std::string get_full_edge_from_num( Size const num );

char get_orientation_from_num( Size const num );

std::string get_full_orientation_from_num( Size const num );

std::string get_full_LW_orientation_from_num( Size const num );

///////////////////////////////////////////////////////////////////////////////
std::string const	first_base_atom( conformation::Residue const & rsd );
bool	is_purine( conformation::Residue const & rsd );
Size first_base_atom_index( conformation::Residue const & rsd );

std::string const	chi1_torsion_atom( conformation::Residue const & rsd );
Size chi1_torsion_atom_index( conformation::Residue const & rsd );

std::string const	default_jump_atom( conformation::Residue const & rsd );

bool
possibly_canonical( chemical::AA const & aa1,  chemical::AA const & aa2 );

bool
possibly_canonical_strict( chemical::AA const & aa1,  chemical::AA const & aa2 );

void
get_watson_crick_base_pair_atoms(
	 chemical::AA const & aa1,
	 chemical::AA const & aa2,
	 std::string & atom1,
	 std::string & atom2 );

void
get_watson_crick_base_pair_atoms(
	 chemical::AA const & aa1,
	 chemical::AA const & aa2,
	 utility::vector1< std::string > & atom_ids1,
	 utility::vector1< std::string > & atom_ids2	 );

bool
is_cutpoint_open( core::pose::Pose const & pose, Size const i );

bool
is_rna_chainbreak( core::pose::Pose const & pose, Size const i );


void
apply_non_main_chain_sugar_coords(
    utility::vector1< Vector > const & non_main_chain_sugar_coords,
		core::pose::Pose & pose,
		core::pose::Pose const & reference_pose,
		core::Size const & i
																	);

void
apply_ideal_c2endo_sugar_coords(
		core::pose::Pose & pose,
		core::Size const & i
																);


	//void
	//apply_torsions_and_non_main_chain_sugar_coords(
	//    utility::vector1< Vector > const & non_main_chain_sugar_coords,
	//		core::pose::Pose & pose,
	//		core::Size const & i,
	//		utility::vector1< Real > const & rna_torsions	);

void
get_fade_correction(
										Real const z,
										Real const cutoff_lower,
										Real const cutoff_upper,
										Real const fade_zone,
										Real & fade_value,
										Real & fade_deriv );


//Copied from Parin SRC on Dec 23, 2011.
numeric::xyzVector<core::Real>
get_rna_base_centroid( core::conformation::Residue const & rsd , bool verbose);

numeric::xyzMatrix< core::Real >
get_rna_base_coordinate_system( core::conformation::Residue const & rsd, numeric::xyzVector<core::Real> const & centroid );

bool
Is_base_phosphate_atom_pair( conformation::Residue const & rsd_1, conformation::Residue const & rsd_2, Size const atomno_1, Size const atomno_2);

utility::vector1< std::string > const &
get_atoms_involved_in_phosphate_torsion();

} //ns rna
} //ns scoring
} //ns core

#endif
