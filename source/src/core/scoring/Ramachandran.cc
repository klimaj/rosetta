// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/scoring/Ramachandran.cc
/// @brief  Ramachandran potential class implementation
/// @author Andrew Leaver-Fay (leaverfa@email.unc.edu)
/// @author Modified by Vikram K. Mulligan (vmullig@uw.edu) for D-amino acids, noncanonical alpha-amino acids, etc.

// Unit Headers
#include <core/scoring/Ramachandran.hh>

// Package Headers
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/Energies.hh>

// Project Headers
#include <core/conformation/Residue.hh>
#include <core/conformation/ppo_torsion_bin.hh>
#include <core/chemical/ResidueConnection.hh>
#include <core/chemical/AA.hh>
#include <core/pose/Pose.hh>
#include <basic/database/open.hh>
#include <basic/options/option.hh>

// Numeric Headers
#include <numeric/angle.functions.hh>
#include <numeric/interpolation/periodic_range/half/interpolation.hh>
#include <numeric/random/random.hh>
#include <numeric/random/random.functions.hh>
#include <numeric/xyz.functions.hh>

// Utility Headers
#include <utility/pointer/ReferenceCount.hh>
#include <utility/io/izstream.hh>

// ObjexxFCL Headers
#include <ObjexxFCL/FArray1D.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/FArray2A.hh>
#include <ObjexxFCL/FArray4D.hh>
#include <ObjexxFCL/string.functions.hh>

// AS -- to get access to get_torsion_bin()
#include <core/conformation/util.hh>

// option key includes

#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <basic/options/keys/corrections.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/OptionKeys.hh>

#include <utility/vector1.hh>
#include <sstream>


using namespace ObjexxFCL;

namespace core {
namespace scoring {


// @brief Auto-generated virtual destructor
Ramachandran::~Ramachandran() {}

typedef Ramachandran R;

Real const R::binw_( 10.0 );
Real const R::rama_sampling_thold_( 0.00075 ); // only sample torsions with Rama prob above this value
Real const R::rama_sampling_factor_( 10.0 ); // factor for increased precision of Rama sampling table


Ramachandran::Ramachandran() :
	ram_probabil_( n_phi_, n_psi_, 3, n_aa_ ),
	ram_counts_( n_phi_, n_psi_, 3, n_aa_ ),
	ram_energ_( n_phi_, n_psi_, 3, n_aa_),
	ram_entropy_( 3, n_aa_ ),
	cdf_( n_aa_ ),
	cdf_by_torsion_bin_( n_aa_, conformation::n_ppo_torsion_bins ),
	n_valid_pp_bins_by_ppo_torbin_( n_aa_, conformation::n_ppo_torsion_bins ),
	phi_psi_bins_above_thold_( n_aa_ )
{
	using namespace basic::options;
	read_rama(
		option[ OptionKeys::corrections::score::rama_map ]().name(),
		option[ OptionKeys::corrections::score::use_bicubic_interpolation ]);
}

Ramachandran::Ramachandran(
	std::string const & rama_map_filename,
	bool use_bicubic_interpolation
) :
	ram_probabil_( n_phi_, n_psi_, 3, n_aa_ ),
	ram_counts_( n_phi_, n_psi_, 3, n_aa_ ),
	ram_energ_( n_phi_, n_psi_, 3, n_aa_),
	ram_entropy_( 3, n_aa_ ),
	cdf_( n_aa_ ),
	cdf_by_torsion_bin_( n_aa_, conformation::n_ppo_torsion_bins ),
	n_valid_pp_bins_by_ppo_torbin_( n_aa_, conformation::n_ppo_torsion_bins ),
	phi_psi_bins_above_thold_( n_aa_ )
{
	read_rama(rama_map_filename, use_bicubic_interpolation);
}

///////////////////////////////////////////////////////////////////////////////

/// @brief Returns true if passed a core::chemical::AA corresponding to a
/// D-amino acid, and false otherwise.
/// @author Vikram K. Mulligan (vmullig@uw.edu)
bool
Ramachandran::is_canonical_d_aminoacid(
	AA const res_aa
) const {
	return core::chemical::is_canonical_D_aa(res_aa);
}

///////////////////////////////////////////////////////////////////////////////

/// @brief When passed a d-amino acid, returns the l-equivalent.  Returns
/// aa_unk otherwise.
/// @author Vikram K. Mulligan (vmullig@uw.edu)
core::chemical::AA
Ramachandran::get_l_equivalent(
	AA const d_aa
) const {
	return core::chemical::get_L_equivalent(d_aa);
}


///////////////////////////////////////////////////////////////////////////////

/// @brief evaluate rama score for each (protein) residue and store that score
/// in the pose.energies() object
void
Ramachandran::eval_rama_score_all(
	pose::Pose & pose,
	ScoreFunction const & scorefxn
) const
{
	if ( scorefxn.has_zero_weight( rama ) ) return; // unnecessary, righ?

	//double rama_sum = 0.0;

	// in pose mode, we use fold_tree.cutpoint info to exclude terminus
	// residues from rama calculation. A cutpoint could be either an artificial
	// cutpoint such as loop cutpoint or a real physical chain break such as
	// multiple-chain complex. For the artificial cutpoint, we may need to
	// calculate rama scores for cutpoint residues, but for the real chain break
	// cutpoint, we don't want to do that. So here we first loop over all the
	// residue in the protein and exclude those ones which are the cutpoints.
	// Then we loop over the cutpoint residues and add rama score for residues
	// at artificial cutpoints, i.e., cut_weight != 0.0, which means that
	// jmp_chainbreak_score is also calculated for this cutpoint. Note that the
	// default value for cut_weight here is dependent on whether
	// jmp_chainbreak_weight is set. This is to ensure that rama score for
	// termini residues are not calculated when jmp_chainbreak_weight is 0.0,
	// e.g normal pose docking.

	int const total_residue = pose.total_residue();

	// retrieve cutpoint info // apl do we actually need this data?
	// if so, Pose must provide it 'cause we're offing all global data
	//
	//kinematics::FoldTree const & fold_tree(
	//		pose.fold_tree() );
	//int const n_cut( fold_tree.num_cutpoint() );

	//FArray1D< Real > cut_weight( n_cut,
	//	scorefxns::jmp_chainbreak_weight == 0.0 ? 0.0 : 1.0 ); // apl need to handle

	//if( cut_weight.size1() == scorefxns::cut_weight.size1() )
	//	cut_weight = scorefxns::cut_weight;

	// exclude chain breaks

	Energies & pose_energies( pose.energies() );

	for ( int ii = 1; ii <= total_residue; ++ii )
	{
		if ( pose.residue(ii).is_protein()  && ! pose.residue(ii).is_terminus() && ! pose.residue(ii).is_virtual_residue() )
		{
			Real rama_score,dphi,dpsi;
			if(is_normally_connected(pose.residue(ii))) {
				eval_rama_score_residue(pose.residue(ii),rama_score,dphi,dpsi);
				//printf("Residue %i is normal.\n", ii); fflush(stdout); //DELETE ME -- FOR TESTING ONLY
				//std::cout << "Rama: residue " << ii << " = " << rama_score << std::endl;
				pose_energies.onebody_energies( ii )[rama] = rama_score;
			} else {
				//printf("Residue %i: THIS SHOULD HAPPEN ONLY IF THIS RESIDUE HAS WEIRD CONNECTIONS.", ii); fflush(stdout); //DELETE ME -- FOR TESTING ONLY
				eval_rama_score_residue_nonstandard_connection(pose, pose.residue(ii),rama_score,dphi,dpsi);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
void
Ramachandran::write_rama_score_all( Pose const & /*pose*/ ) const
{}


///////////////////////////////////////////////////////////////////////////////
/// Sample phi/psi torsions with probabilities proportionate to their
/// Ramachandran probabilities
/// Note -- this function had previously required that the option
/// loops::nonpivot_torsion_sampling be active.  This function now
/// performs a just-in-time check to initialize these tables the first
/// time they are requested -- To properly multi-thread this code, the
/// function should nab a mutex so that no two threads try to execute
/// the code at once.
/// Note2 -- this function has been hackily patched to allow it to be used for
/// D-amino acids. (VKM -- 21 Aug 2014).
void
Ramachandran::random_phipsi_from_rama(
	AA const res_aa,
	Real & phi,
	Real & psi
) const
{
	AA res_aa2 = res_aa;
	core::Real phipsi_multiplier=1.0;
	if(is_canonical_d_aminoacid(res_aa)) {
		res_aa2=get_l_equivalent(res_aa);
		phipsi_multiplier=-1.0;
	}
	draw_random_phi_psi_from_cdf( cdf_[ res_aa2 ], phi, psi );
	phi *= phipsi_multiplier;
	psi *= phipsi_multiplier;
	return;
}

void
Ramachandran::uniform_phipsi_from_allowed_rama(
	AA const res_aa,
	Real & phi,
	Real & psi
) const
{
	using numeric::random::uniform;
	using numeric::random::random_range;

	Size n_torsions = phi_psi_bins_above_thold_[res_aa].size();
	Size random_index = random_range(1, n_torsions);
	Size ppbin_index = phi_psi_bins_above_thold_[ res_aa ][ random_index ];
	Size phi_index = ppbin_index / n_psi_;
	Size psi_index = ppbin_index - phi_index * n_psi_;

	phi = binw_ * ( phi_index + uniform());
	psi = binw_ * ( psi_index + uniform());
}

/// @brief return true if the bin that the given phi/psi pair falls in has a
/// probability that exceeds the rama_sampling_thold_ constant.
bool
Ramachandran::phipsi_in_allowed_rama(
	AA const aa,
	Real phi,
	Real psi
) const
{
	phi = numeric::nonnegative_principal_angle_degrees(phi);
	psi = numeric::nonnegative_principal_angle_degrees(psi);

	Size phi_bin = static_cast< Size > ( phi / binw_ );
	Size psi_bin = static_cast< Size > ( psi / binw_ );

	return ram_probabil_(phi_bin+1, psi_bin+1, 3, aa ) > rama_sampling_thold_;
}

bool
Ramachandran::phipsi_in_forbidden_rama(
	AA const aa,
	Real phi,
	Real psi
) const
{
	return ! phipsi_in_allowed_rama(aa, phi, psi);
}

///////////////////////////////////////////////////////////////////////////////
/// Sample phi/psi torsions with probabilities proportionate to their
/// Ramachandran probabilities -- this version performs lookup restricted to specified torsion bins
/// based on random_phipsi_from_rama and has the same issue for parallel running

/// @author Amelie Stein (amelie.stein@ucsf.edu)
/// @date Fri May 11 15:52:01 PDT 2012
/// @details returns a random phi/psi combination within the given torsion bin -- WARNING: this will only work for the torsion bins that are currently implemented

void
Ramachandran::random_phipsi_from_rama_by_torsion_bin(
	AA const res_aa,
	Real & phi,
	Real & psi,
	conformation::ppo_torsion_bin torsion_bin
) const
{
	draw_random_phi_psi_from_cdf( cdf_by_torsion_bin_( res_aa, torsion_bin ), phi, psi );
}


void
Ramachandran::get_entries_per_torsion_bin(
	AA const res_aa,
	std::map< conformation::ppo_torsion_bin, core::Size > & tb_frequencies
) const
{
	tb_frequencies[ conformation::ppo_torbin_A ] = phi_psi_bins_above_thold_[ res_aa ][ conformation::ppo_torbin_A ];
	tb_frequencies[ conformation::ppo_torbin_B ] = phi_psi_bins_above_thold_[ res_aa ][ conformation::ppo_torbin_B ];
	tb_frequencies[ conformation::ppo_torbin_E ] = phi_psi_bins_above_thold_[ res_aa ][ conformation::ppo_torbin_E ];
	tb_frequencies[ conformation::ppo_torbin_G ] = phi_psi_bins_above_thold_[ res_aa ][ conformation::ppo_torbin_G ];
	tb_frequencies[ conformation::ppo_torbin_X ] = phi_psi_bins_above_thold_[ res_aa ][ conformation::ppo_torbin_X ];
}


///////////////////////////////////////////////////////////////////////////////
void
Ramachandran::eval_rama_score_residue_nonstandard_connection(
	core::pose::Pose const & mypose,
	conformation::Residue const & res,
	Real & rama,
	Real & drama_dphi,
	Real & drama_dpsi
) const {

	if(res.connection_incomplete(1) || res.connection_incomplete(2)) { //If this is an open terminus, don't score this residue.
		rama=0.0;
		drama_dphi=0.0;
		drama_dpsi=0.0;
		return;
	}

	core::conformation::Residue const & lowerres = mypose.residue(res.residue_connection_partner(1));
	core::conformation::Residue const & upperres = mypose.residue(res.residue_connection_partner(2));

	// NOTE: The following assumes that we're scoring something with an alpha-amino acid backbone!  At the time
	// of this writing (7 Feb 2013), rama checks that the residue being scored is either a standard L- or D-amino acid.
	Real phi = numeric::nonnegative_principal_angle_degrees(
		numeric::dihedral_degrees(
		lowerres.xyz( lowerres.residue_connect_atom_index(res.residue_connection_conn_id(1)) ),
		res.xyz("N"), //Position of N
		res.xyz("CA"), //Position of CA
		res.xyz("C") //Position of C
		));

	Real psi=numeric::nonnegative_principal_angle_degrees(
		numeric::dihedral_degrees(
		res.xyz("N"), //Position of N
		res.xyz("CA"), //Position of CA
		res.xyz("C"), //Position of C
		upperres.xyz( upperres.residue_connect_atom_index(res.residue_connection_conn_id(2)) )
		));

	//printf("rsd %lu phi=%.3f psi=%.3f\n", res.seqpos(), phi, psi); fflush(stdout); //DELETE ME

	core::chemical::AA ref_aa = res.aa();
	if(res.backbone_aa() != core::chemical::aa_unk) ref_aa = res.backbone_aa(); //If this is a noncanonical that specifies a canonical to use as a Rama template, use the template.

	eval_rama_score_residue( ref_aa, phi, psi, rama, drama_dphi, drama_dpsi );

	return;
}

///////////////////////////////////////////////////////////////////////////////
Real
Ramachandran::eval_rama_score_residue(
	conformation::Residue const & rsd
) const
{
	Real rama, drama_dphi, drama_dpsi;
	eval_rama_score_residue( rsd, rama, drama_dphi, drama_dpsi );
	return rama;
}

///////////////////////////////////////////////////////////////////////////////
void
Ramachandran::eval_rama_score_residue(
	conformation::Residue const & rsd,
	Real & rama,
	Real & drama_dphi,
	Real & drama_dpsi
) const
{
	using namespace numeric;

	debug_assert( rsd.is_protein() );

	if ( 0.0 == nonnegative_principal_angle_degrees( rsd.mainchain_torsion(1) ) ||
			0.0 == nonnegative_principal_angle_degrees( rsd.mainchain_torsion(2) ) ||
			rsd.is_terminus() ||
			rsd.is_virtual_residue() ) { // begin or end of chain -- don't calculate rama score
		rama = 0.0;
		drama_dphi = 0.0;
		drama_dpsi = 0.0;
		return;
	}

	Real phi=0.0;
	Real psi=0.0;


	if ( is_normally_connected(rsd) ) { //If this residue is conventionally connected
		phi = nonnegative_principal_angle_degrees( rsd.mainchain_torsion(1) );
		psi = nonnegative_principal_angle_degrees( rsd.mainchain_torsion(2) );
		core::chemical::AA ref_aa = rsd.aa();
		if ( rsd.backbone_aa() != core::chemical::aa_unk ) {
			ref_aa = rsd.backbone_aa(); //If this is a noncanonical that specifies a canonical to use as a Rama template, use the template.
		}

		eval_rama_score_residue( ref_aa, phi, psi, rama, drama_dphi, drama_dpsi );
	} else { //If this residue is unconventionally connected (should be handled elsewhere)
		//printf("Residue %lu: THIS SHOULD NEVER OCCUR!\n", rsd.seqpos()); fflush(stdout); //DELETE ME -- FOR TESTING ONLY
		rama = 0.0;
		drama_dphi = 0.0;
		drama_dpsi = 0.0;
		return;
	}

	return;
}


///////////////////////////////////////////////////////////////////////////////
///
Real
Ramachandran::eval_rama_score_residue(
	AA const res_aa,
	Real const phi,
	Real const psi
) const
{

	Real rama, drama_dphi, drama_dpsi;
	eval_rama_score_residue( res_aa, phi, psi, rama, drama_dphi, drama_dpsi );
	return rama;
}

void
Ramachandran::eval_rama_score_residue(
	AA const res_aa,
	Real const phi,
	Real const psi,
	Real & rama,
	Real & drama_dphi,
	Real & drama_dpsi
) const {
	using namespace basic::options;
	eval_rama_score_residue(
		option[ OptionKeys::corrections::score::use_bicubic_interpolation ],
		option[ OptionKeys::corrections::score::rama_not_squared ],
		res_aa, phi, psi, rama, drama_dphi, drama_dpsi);
}

///////////////////////////////////////////////////////////////////////////////
///
void
Ramachandran::eval_rama_score_residue(
	bool use_bicubic_interpolation,
	bool rama_not_squared,
	AA const res_aa,
	Real const phi,
	Real const psi,
	Real & rama,
	Real & drama_dphi,
	Real & drama_dpsi
) const
{
	using namespace numeric;

//db
//db secondary structure dependent tables favor helix slightly.
//db only use if have predicted all alpha protein
//db
// rhiju and db: no longer use alpha-specific rama, after
//  tests on 1yrf and other all alpha proteins. 2-8-07

// apl -- removing ss dependence on rama in first implementation of mini
// after reading rhiju and david's comment above.  We will need a structural annotation
// obect (structure.cc, maybe a class SecStruct) at some point.  The question
// remains whether a pose should hold that object and be responsible for its upkeep,
// or whether such an object could be created as needed to sit alongside a pose.
//
//	std::string protein_sstype = get_protein_sstype();
//	int ss_type;
//	if ( use_alpha_rama_flag() && get_protein_sstype() == "a" ) {
//		ss_type = ( ( ss == 'H' ) ? 1 : ( ( ss == 'E' ) ? 2 : 3 ) );
//	} else {
	int ss_type = 3;
//	}

//     do I (?cems/cj/cdb?) want to interpolate probabilities or log probs???
//     currently am interpolating  probs then logging.

	//int const res_aa( rsd.aa() );
	// 	int const res_aa( pose.residue( res ).aa() );

	core::chemical::AA res_aa2 = res_aa;
	core::Real phi2 = phi;
	core::Real psi2 = psi;
	core::Real d_multiplier=1.0; //A multiplier for derivatives: 1.0 for L-amino acids, -1.0 for D-amino acids.

	if ( is_canonical_d_aminoacid( res_aa ) ) { //If this is a D-amino acid, invert phi and psi and use the corresponding L-amino acid for the calculation
		res_aa2 = get_l_equivalent( res_aa );
		phi2 = -phi;
		psi2 = -psi;
		d_multiplier = -1.0;
	}

	if ( use_bicubic_interpolation ) {

		rama = rama_energy_splines_[ res_aa2 ].F(phi2,psi2);
		drama_dphi = d_multiplier*rama_energy_splines_[ res_aa2 ].dFdx(phi2,psi2);
		drama_dpsi = d_multiplier*rama_energy_splines_[ res_aa2 ].dFdy(phi2,psi2);
		//printf("drama_dphi=%.8f\tdrama_dpsi=%.8f\n", drama_dphi, drama_dpsi); //DELETE ME!
		//if(is_canonical_d_aminoacid(res_aa)) { printf("rama = %.4f\n", rama); fflush(stdout); } //DELETE ME!
		return; // temp -- just stop right here
	} else {

		FArray2A< Real >::IR const zero_index( 0, n_phi_ - 1);
		FArray2A< Real > const rama_for_res( ram_probabil_(1, 1, ss_type, res_aa2), zero_index, zero_index );
		Real interp_p,dp_dphi,dp_dpsi;

		using namespace numeric::interpolation::periodic_range::half;
		interp_p = bilinearly_interpolated( phi2, psi2, binw_, n_phi_, rama_for_res, dp_dphi, dp_dpsi );

		if ( interp_p > 0.0 ) {
			rama = ram_entropy_(ss_type, res_aa2 ) - std::log( static_cast< double >( interp_p ) );
			double const interp_p_inv_neg = -1.0 / interp_p;
			drama_dphi = interp_p_inv_neg * d_multiplier * dp_dphi;
			drama_dpsi = interp_p_inv_neg * d_multiplier * dp_dpsi;
		} else {
			//if ( runlevel > silent ) { //apl fix this
			//	std::cout << "rama prob = 0. in eval_rama_score_residue!" << std::endl;
			//	std::cout << "phi" << SS( phi ) << " psi" << SS( psi ) <<
			//	 " ss " << SS( ss ) << std::endl;
			//}
			drama_dphi = 0.0;
			drama_dpsi = 0.0;
			rama = 20.0;
		}

		if ( ! rama_not_squared ) {
			if ( rama > 1.0 ) {
				Real const rama_squared = rama * rama;
				if ( rama_squared > 20.0 ) {
					////  limit the score, but give the true derivative
					////   as guidance out of the flat section of map
					drama_dphi = 0.0;
					drama_dpsi = 0.0;
					rama = 20.0;
				} else {
					drama_dphi = 2 * rama * drama_dphi;
					drama_dpsi = 2 * rama * drama_dpsi;
					rama = rama_squared;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//void Ramachandran::eval_procheck_rama(
//	Pose const & /*pose*/,
//	Real & /*favorable*/,
//	Real & /*allowed*/,
//	Real & /*generous*/
//) const
//{}

bool
Ramachandran::is_normally_connected (
	conformation::Residue const & res
) const {

	if ( res.is_upper_terminus() || res.is_lower_terminus() || res.connect_map_size() < 2 ) {
		return true; //Termini register as normally connected since they're not to be scored by rama.
	}

	bool firstconn = true, secondconn=true;

	if(!res.connection_incomplete(1)) firstconn= (res.residue_connection_partner(1) == res.seqpos()-1);
	if(!res.connection_incomplete(2)) secondconn= (res.residue_connection_partner(2) == res.seqpos()+1);

	return (firstconn && secondconn);
}

Size Ramachandran::n_phi_bins() const { return n_phi_; }

Size Ramachandran::n_psi_bins() const { return n_psi_; }

Real Ramachandran::rama_probability( core::chemical::AA aa, Real phi, Real psi ) const {
	phi = numeric::nonnegative_principal_angle_degrees( phi );
	psi = numeric::nonnegative_principal_angle_degrees( psi );
	Size phi_ind = static_cast< Size > ( floor( phi / binw_ ) ) + 1;
	Size psi_ind = static_cast< Size > ( floor( psi / binw_ ) ) + 1;
	return ram_probabil_( phi_ind, psi_ind, 3, aa );
}

Real Ramachandran::minimum_sampling_probability() const {
	return rama_sampling_thold_;
}


utility::vector1< Real > const &
Ramachandran::cdf_for_aa( core::chemical::AA aa ) const
{
	return cdf_[ aa ];
}

utility::vector1< Real > const &
Ramachandran::cdf_for_aa_for_torsion_bin(
	chemical::AA aa,
	conformation::ppo_torsion_bin torsion_bin
) const
{
	return cdf_by_torsion_bin_( aa, torsion_bin );
}


void
Ramachandran::read_rama_map_file (
	utility::io::izstream * iunit
) {
	int aa_num,phi_bin,psi_bin,ss_type;
	Real check,min_prob,max_prob;
	double entropy;
	char line[60];
	int scan_count;
	float pval, eval; // vars for sscanf float I/O

	for ( int i = 1; i <= n_aa_ ; ++i ) {
		for ( int ii = 1; ii <= 3; ++ii ) {
			entropy = 0.0;
			check = 0.0;
			min_prob = 1e36;
			max_prob = -min_prob;
			for ( int j = 1; j <= 36; ++j ) {
				for ( int k = 1; k <= 36; ++k ) {
					iunit->getline( line, 60 );
					if ( iunit->eof() ) {
						return;
					} else if ( iunit->fail() ) { // Clear and continue: NO ERROR DETECTION
						iunit->clear();
					}
					std::sscanf( line, "%5d", &aa_num );
					std::sscanf( line+6, "%5d", &ss_type );
					std::sscanf( line+12, "%5d", &phi_bin );
					std::sscanf( line+18, "%5d", &psi_bin );
					std::sscanf( line+24, "%5d", &ram_counts_(j,k,ii,i) );
					std::sscanf( line+30, "%12f", &pval );
					ram_probabil_(j,k,ii,i) = pval;
					scan_count = std::sscanf( line+43, "%12f", &eval );
					ram_energ_(j,k,ii,i) = eval;

					if ( scan_count == EOF ) continue; // Read problem: NO ERROR DETECTION

// This is the Slick & Slow (S&S) stream-based method that is too slow for large
// files like this one, at least under the GCC 3.3.1 stream implementation.
// It should be retried on future releases and target compilers because there is
// no reason it cannot be competitive with good optimization and inlining.
// If this is used the <cstdio> can be removed.
//
//					iunit >> bite( 5, aa_num ) >> skip( 1 ) >>
//					 bite( 5, ss_type ) >> skip( 1 ) >>
//					 bite( 5, phi_bin ) >> skip( 1 ) >>
//					 bite( 5, psi_bin ) >> skip( 1 ) >>
//					 bite( 5, ram_counts(j,k,ii,i) ) >> skip( 1 ) >>
//					 bite( 12, ram_probabil(j,k,ii,i) ) >> skip( 1 ) >>
//					 bite( 12, ram_energ(j,k,ii,i) ) >> skip;
//					if ( iunit.eof() ) {
//						goto L100;
//					} else if ( iunit.fail() ) { // Clear and continue: NO ERROR DETECTION
//						iunit.clear();
//						iunit >> skip;
//					}

					check += ram_probabil_(j,k,ii,i);
					entropy += ram_probabil_(j,k,ii,i) *
					 std::log( static_cast< double >( ram_probabil_(j,k,ii,i) ) );
					min_prob = std::min(ram_probabil_(j,k,ii,i),min_prob);
					max_prob = std::max(ram_probabil_(j,k,ii,i),max_prob);
				}
			}
			ram_entropy_(ii,i) = entropy;
		}
//cj		std::cout << SS( check ) << SS( std::log(min_prob) ) <<
//cj		 SS( std::log(max_prob) ) << SS( entropy ) << std::endl;
	}

}

void
Ramachandran::read_rama(
	std::string const & rama_map_filename,
	bool use_bicubic_interpolation
) {

	utility::io::izstream  iunit;

  // search in the local directory first
  iunit.open( rama_map_filename );

  if ( !iunit.good() ) {
    iunit.close();
    if(!basic::database::open( iunit, rama_map_filename )){
			std::stringstream err_msg;
			err_msg << "Unable to open Ramachandran map '" << rama_map_filename << "'.";
			utility_exit_with_message(err_msg.str());
		}
  }

//cj      std::cout << "index" << "aa" << "ramachandran entropy" << std::endl;
//KMa add_phospho_ser 2006-01
	read_rama_map_file (&iunit);

	iunit.close();
	iunit.clear();

	if ( use_bicubic_interpolation ) {
		using namespace numeric;
		using namespace numeric::interpolation::spline;
		rama_energy_splines_.resize( chemical::num_canonical_aas );
		for ( Size ii = 1; ii <= chemical::num_canonical_aas; ++ii ) {
			BicubicSpline ramaEspline;
			MathMatrix< Real > energy_vals( 36, 36 );
			for ( Size jj = 0; jj < 36; ++jj ) {
				for ( Size kk = 0; kk < 36; ++kk ) {
					energy_vals( jj, kk ) = -std::log( ram_probabil_(jj+1,kk+1,3,ii )) + ram_entropy_(3,ii) ;
				}
			}
			BorderFlag periodic_boundary[2] = { e_Periodic, e_Periodic };
			Real start_vals[2] = {5.0, 5.0}; // grid is shifted by five degrees.
			Real deltas[2] = {10.0, 10.0}; // grid is 10 degrees wide
			bool lincont[2] = {false,false}; //meaningless argument for a bicubic spline with periodic boundary conditions
			std::pair< Real, Real > unused[2];
			unused[0] = std::make_pair( 0.0, 0.0 );
			unused[1] = std::make_pair( 0.0, 0.0 );
			ramaEspline.train( periodic_boundary, start_vals, deltas, energy_vals, lincont, unused );
			rama_energy_splines_[ ii ] = ramaEspline;
		}
	}

	initialize_rama_sampling_tables();
}

void
Ramachandran::initialize_rama_sampling_tables()
{
	init_rama_sampling_table( conformation::ppo_torbin_X );
	init_rama_sampling_table( conformation::ppo_torbin_A );
	init_rama_sampling_table( conformation::ppo_torbin_B );
	init_rama_sampling_table( conformation::ppo_torbin_E );
	init_rama_sampling_table( conformation::ppo_torbin_G );

	init_uniform_sampling_table();
}

void
Ramachandran::init_rama_sampling_table( conformation::ppo_torsion_bin torsion_bin )
{
	utility::vector1< Real > inner_cdf( n_phi_ * n_psi_, 0.0 );
	FArray2A< Real >::IR const zero_index( 0, n_phi_ - 1);
	for ( int ii = 1; ii <= n_aa_; ++ii ) {
		std::fill( inner_cdf.begin(), inner_cdf.end(), Real( 0.0 ) );
		FArray2A< Real > const ii_rama_prob( ram_probabil_( 1, 1, 3, ii ), zero_index, zero_index );

		Size actual_allowed( 0 );
		Real allowed_probability_sum( 0.0 );
		for ( int jj = 0; jj < n_phi_; ++jj ) {
			for ( int kk = 0; kk < n_psi_; ++kk ) {

				inner_cdf[ jj*n_psi_ + kk +  1 ] = allowed_probability_sum;

				Real jjkk_prob = ii_rama_prob( jj, kk );
				if ( jjkk_prob < rama_sampling_thold_ ) continue;
				Real const cur_phi = binw_ * ( jj - ( jj > n_phi_ / 2 ? n_phi_ : 0 ));
				Real const cur_psi = binw_ * ( kk - ( kk > n_psi_ / 2 ? n_psi_ : 0 ));


				conformation::ppo_torsion_bin cur_tb = conformation::ppo_torbin_X;
				if (torsion_bin != conformation::ppo_torbin_X) {
					//  AS -- how can we get the factor properly / without hard-coding? - also: this takes very long...
					cur_tb = core::conformation::get_torsion_bin(cur_phi, cur_psi);
				}

				//std::cout << "    " << ii << " " << cur_phi << " " << cur_psi << " " << core::conformation::map_torsion_bin_to_char( cur_tb ) << " " << jjkk_prob << std::endl;

				if ( cur_tb == torsion_bin ) {
					++actual_allowed;
					// store the cumulative sum up to the current table entry
					// then increment the cumulative sum with the current table entry
					allowed_probability_sum += jjkk_prob;
				}

			}
		}

		//std::cout << "Ramachandran: " << ii << " " << core::conformation::map_torsion_bin_to_char( torsion_bin ) << " " << actual_allowed << std::endl;

		if ( actual_allowed == 0 ) {
			// store a vector of 0s instead of creating a vector of NaNs.
			cdf_by_torsion_bin_( ii, torsion_bin ) = inner_cdf;
			continue;
		}

		// now normalize the cdf vector by scaling by 1/allowed_probability_sum so that it represents a CDF that sums to 1.
		// and mark all zero-probability bins with the CDF value of their predecessor.
		Real const inv_allowed_probability_sum = 1 / allowed_probability_sum;
		for ( int jj = 1; jj <= n_phi_ * n_psi_; ++jj ) {
			inner_cdf[ jj ] *= inv_allowed_probability_sum;
			//if ( inner_cdf[ jj ] == 0 && jj != 1 ) {
			//	inner_cdf[ jj ] = inner_cdf[ jj-1 ];
			//}
		}

		// now update the cdf, cdf_by_torsion_bin, and n_valid_pp_bins_by_ppo_torbin tables
		if ( torsion_bin == conformation::ppo_torbin_X ) {
			cdf_[ ii ] = inner_cdf;
		}
		cdf_by_torsion_bin_( ii, torsion_bin ) = inner_cdf;
		n_valid_pp_bins_by_ppo_torbin_( ii, torsion_bin ) = actual_allowed;

	}

}

void
Ramachandran::init_uniform_sampling_table()
{
	FArray2A< Real >::IR const zero_index( 0, n_phi_ - 1);
	for ( int ii = 1; ii <= n_aa_; ++ii ) {
		utility::vector1< Size > minimum_prob_bin_inds;
		minimum_prob_bin_inds.reserve( n_phi_ * n_psi_ );
		FArray2A< Real > const ii_rama_prob( ram_probabil_( 1, 1, 3, ii ), zero_index, zero_index );
		for ( int jj = 0; jj < n_phi_; ++jj ) {
			for ( int kk = 0; kk < n_psi_; ++kk ) {
				Real jjkk_prob = ii_rama_prob( jj, kk );
				if ( jjkk_prob >= rama_sampling_thold_ ) {
					minimum_prob_bin_inds.push_back( jj*n_psi_ + kk );
				}
			}
		}
		phi_psi_bins_above_thold_[ ii ] = minimum_prob_bin_inds;
	}
}

void
Ramachandran::draw_random_phi_psi_from_cdf(
	utility::vector1< Real > const & cdf,
	Real & phi,
	Real & psi
) const
{
	// the bin index can be unpacked to give the phi and psi indices
	Size bin_from_cdf = numeric::random::pick_random_index_from_cdf( cdf, numeric::random::rg() );
	--bin_from_cdf;

	Size phi_ind = bin_from_cdf / n_psi_;
	Size psi_ind = bin_from_cdf - phi_ind * n_phi_;

	//std::cout << " phi_ind: " << phi_ind << " psi_ind: " << psi_ind << std::endl;

	// following lines set phi and set to values drawn proportionately from Rama space
	// AS Nov 2013 - note that phi/psi bins are lower left corners, so we only want to add "noise" from a uniform distribution
	phi = binw_ * ( phi_ind + numeric::random::uniform() );
	psi = binw_ * ( psi_ind + numeric::random::uniform() );
}


}
}
