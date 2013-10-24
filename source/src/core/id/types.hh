// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/id/types.hh
/// @brief  core::id package type declarations
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)


#ifndef INCLUDED_core_id_types_hh
#define INCLUDED_core_id_types_hh


// Package headers
#include <core/types.hh>


namespace core {
namespace id {


// Types

/// @brief DOF (degrees of freedom) type
/// - PHI: torsion or improper angle
/// - THETA: bond angle
/// - D: distance
/// - RB1-RB6: rigid-body jump translation and rotation
enum DOF_Type {
	PHI = 1, // used for lookup into utility::vector1
	THETA,
	D,
	RB1,
	RB2,
	RB3,
	RB4,
	RB5,
	RB6
};
static Size const n_DOF_Type( 9 ); // Update this if DOF_Type changes

/// @brief Torsion type -- used in the TorsionID class
/// - BB: backbone torsion
/// - CHI: sidechain torsion
/// - NU: internal ring torsion
/// - JUMP: rigid-body transformation
enum TorsionType {
	BB = 1,
	CHI,
	NU,
	JUMP
};

static Size const   phi_torsion( 1 );
static Size const   psi_torsion( 2 );
static Size const omega_torsion( 3 );

} // namespace id
} // namespace core


#endif // INCLUDED_core_id_types_HH
