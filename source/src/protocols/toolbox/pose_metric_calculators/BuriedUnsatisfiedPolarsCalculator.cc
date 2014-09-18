// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.
//////////////////////////////////////////////////////////////////////
/// @begin BuriedUnsatisfiedPolarsCalculator
///
/// @brief
/// How many buried unsatisfied polars are there?
///
/// @detailed
/// Buried unsatisfied polar hbonds are destabilizing for proteins. It is good to have less.
/// In a study of 2299 high resolution crystal structures of 1.5A or better, there was an average
/// 71 unsatisfied buried polar hbonds. The normalized average (normalized against aa #) was 0.30 (unpublished).
/// To get this piece of code to work, you must first load in your pdb. Then, you need the following lines:
///
///	core::pose::metrics::PoseMetricCalculatorOP sasa_calculator = new protocols::toolbox::PoseMetricCalculators::SasaCalculatorLegacy;
///	core::pose::metrics::CalculatorFactory::Instance().register_calculator( "sasa", sasa_calculator );
///
///	core::pose::metrics::PoseMetricCalculatorOP num_hbonds_calculator = new protocols::toolbox::PoseMetricCalculators::NumberHBondsCalculator();
///	core::pose::metrics::CalculatorFactory::Instance().register_calculator( "num_hbonds", num_hbonds_calculator );
///
///	core::pose::metrics::PoseMetricCalculatorOP unsat_calculator = new protocols::toolbox::PoseMetricCalculators::BuriedUnsatisfiedPolarsCalculator("sasa", "num_hbonds");
///	core::pose::metrics::CalculatorFactory::Instance().register_calculator( "unsat", unsat_calculator );
///
/// This segment of code sets everything up to be used in the calculator. To use this on your protein, you simply need to
/// write the following: pose.print_metric("unsat", "all_bur_unsat_polars");
///
/// @author
/// Florian Richter
/// Steven Combs - comments
///
/// @last_modified November 19 2010
/////////////////////////////////////////////////////////////////////////
/// @file   core/pose/metrics/BuriedUnsatisfiedPolarsCalculator.cc
/// @brief  number of hbonds calculator class
/// @author Florian Richter

// Unit headers
#include <protocols/toolbox/pose_metric_calculators/BuriedUnsatisfiedPolarsCalculator.hh>

#include <core/conformation/Residue.hh>
#include <core/pose/metrics/CalculatorFactory.hh>
#include <core/pose/Pose.hh>
#include <protocols/toolbox/pose_metric_calculators/NumberHBondsCalculator.hh>
#include <core/pose/metrics/simple_calculators/SasaCalculatorLegacy.hh>




// Utility headers
#include <basic/Tracer.hh>
#include <utility/exit.hh>
#include <utility/stream_util.hh>
#include <utility/string_util.hh>
#include <basic/MetricValue.hh>


#include <cassert>

#include <core/chemical/AtomType.hh>
#include <utility/vector1.hh>



using namespace core;
using namespace core::pose;
using namespace core::pose::metrics;

static thread_local basic::Tracer TR( "protocols.toolbox.PoseMetricCalculators.BuriedUnsatisfiedPolarsCalculator" );

namespace protocols{
namespace toolbox {
namespace pose_metric_calculators {

BuriedUnsatisfiedPolarsCalculator::BuriedUnsatisfiedPolarsCalculator(
  std::string sasa_calc,
  std::string hbond_calc,
  core::Real burial_cutoff
) : all_bur_unsat_polars_( 0 ),
    special_region_bur_unsat_polars_(0),
    name_of_hbond_calc_( hbond_calc ),
    name_of_sasa_calc_( sasa_calc ),
    burial_sasa_cutoff_( burial_cutoff )
{
  atom_bur_unsat_.clear();
  residue_bur_unsat_polars_.clear();
  special_region_.clear();
  assert_calculators();

}


BuriedUnsatisfiedPolarsCalculator::BuriedUnsatisfiedPolarsCalculator(
  std::string sasa_calc,
  std::string hbond_calc,
  std::set< core::Size > const & special_region,
  core::Real burial_cutoff
) : all_bur_unsat_polars_(0),
    special_region_bur_unsat_polars_(0),
    name_of_hbond_calc_( hbond_calc ),
    name_of_sasa_calc_( sasa_calc ),
    burial_sasa_cutoff_( burial_cutoff ),
    special_region_( special_region )
{
  atom_bur_unsat_.clear();
  residue_bur_unsat_polars_.clear();
  assert_calculators();
}


void
BuriedUnsatisfiedPolarsCalculator::assert_calculators()
{
	if( !CalculatorFactory::Instance().check_calculator_exists( name_of_hbond_calc_ ) ){
		if( name_of_hbond_calc_ != "default" ) TR << "Attention: couldn't find the specified hbond calculator ( " << name_of_hbond_calc_ << " ), instantiating default one." << std::endl;
		name_of_hbond_calc_ = "bur_unsat_calc_default_hbond_calc";
		if( !CalculatorFactory::Instance().check_calculator_exists( name_of_hbond_calc_ ) ){
			CalculatorFactory::Instance().register_calculator( name_of_hbond_calc_, new NumberHBondsCalculator() );
		}
	}

	if( !CalculatorFactory::Instance().check_calculator_exists( name_of_sasa_calc_ ) ){
		if( name_of_sasa_calc_ != "default" ) TR << "Attention: couldn't find the specified sasa calculator ( " << name_of_sasa_calc_ << " ), instantiating default one." << std::endl;
		name_of_sasa_calc_ = "bur_unsat_calc_default_sasa_calc";
		if( !CalculatorFactory::Instance().check_calculator_exists( name_of_sasa_calc_ ) ){
			CalculatorFactory::Instance().register_calculator( name_of_sasa_calc_, new core::pose::metrics::simple_calculators::SasaCalculatorLegacy() );
		}
	}
}


void
BuriedUnsatisfiedPolarsCalculator::lookup(
  std::string const & key,
  basic::MetricValueBase * valptr
) const
{

   if ( key == "all_bur_unsat_polars" ) {
     basic::check_cast( valptr, &all_bur_unsat_polars_, "all_bur_unsat_polars expects to return a Size" );
     (static_cast<basic::MetricValue<Size> *>(valptr))->set( all_bur_unsat_polars_ );

   } else if ( key == "special_region_bur_unsat_polars" ) {
     basic::check_cast( valptr, &special_region_bur_unsat_polars_, "special_region_bur_unsat_polars expects to return a Size" );
     (static_cast<basic::MetricValue<Size> *>(valptr))->set( special_region_bur_unsat_polars_ );

   } else if ( key == "atom_bur_unsat" ) {
     basic::check_cast( valptr, &atom_bur_unsat_, "atom_bur_unsat expects to return a id::AtomID_Map< bool >" );
     (static_cast<basic::MetricValue<id::AtomID_Map< bool > > *>(valptr))->set( atom_bur_unsat_ );

   } else if ( key == "residue_bur_unsat_polars" ) {
     basic::check_cast( valptr, &residue_bur_unsat_polars_, "residue_bur_unsat_polars expects to return a utility::vector1< Size >" );
     (static_cast<basic::MetricValue<utility::vector1< Size > > *>(valptr))->set( residue_bur_unsat_polars_ );

   } else {
     basic::Error() << "NumberHbondsCalculator cannot compute the requested metric " << key << std::endl;
     utility_exit();
   }

} //lookup



std::string
BuriedUnsatisfiedPolarsCalculator::print( std::string const & key ) const
{

  if ( key == "all_bur_unsat_polars" ) {
    return utility::to_string( all_bur_unsat_polars_ );
  } else if ( key == "special_region_bur_unsat_polars" ) {
    return utility::to_string( special_region_bur_unsat_polars_ );
  } else if ( key == "atom_Hbonds" ) {
    basic::Error() << "id::AtomID_Map< bool > has no output operator, for metric " << key << std::endl;
    utility_exit();
  } else if ( key == "residue_bur_unsat_polars" ) {
    return utility::to_string( residue_bur_unsat_polars_ );
  }

  basic::Error() << "NumberHbondsCalculator cannot compute metric " << key << std::endl;
  utility_exit();
  return "";

} //print


/// @brief this function doesn't actually recompute anything by itself, but calls the
/// @brief two member calculators and then processes the information out of the two of them
void
BuriedUnsatisfiedPolarsCalculator::recompute( Pose const & this_pose )
{

	all_bur_unsat_polars_ = 0;
	special_region_bur_unsat_polars_ = 0;

	if( this_pose.total_residue() != residue_bur_unsat_polars_.size() ){
		residue_bur_unsat_polars_.resize( this_pose.total_residue() );
		atom_bur_unsat_.resize( this_pose.total_residue() );
	}

	basic::MetricValue< id::AtomID_Map< Real > > atom_sasa;
	basic::MetricValue< id::AtomID_Map< Size > > atom_hbonds;



	this_pose.metric( name_of_hbond_calc_, "atom_Hbonds", atom_hbonds );
	this_pose.metric( name_of_sasa_calc_, "atom_sasa", atom_sasa);

	for( Size i = 1; i <= this_pose.total_residue(); ++i){

		residue_bur_unsat_polars_[i] = 0;

		conformation::Residue const & rsd = this_pose.residue( i );

		//utility::vector1< Real > const & atom_sasas_this_res = atom_sasa.value()[ i ];
		//utility::vector1< Size > const & atom_hbonds_this_res = atom_hbonds.value()[ i ];

		for( Size at = 1; at <= rsd.nheavyatoms(); ++at){

			core::id::AtomID atid( at, i );
			bool this_atom_bur_unsat(false);

			if( rsd.atom_type( at ).is_acceptor() || rsd.atom_type( at ).is_donor() ){
				//we have to add up the sasas for the H attached to this atom
				Real cursasa =  atom_sasa.value()[ atid ];
				for( Size hcount = rsd.type().attached_H_begin( at ); hcount<= rsd.type().attached_H_end( at ); hcount++){
					cursasa = cursasa + atom_sasa.value()[ core::id::AtomID ( hcount, i ) ];
				}

				if( cursasa < burial_sasa_cutoff_ ){
					Size satisfac_cut = satisfaction_cutoff( rsd.type().atom_type( at ).name() );
					Size bonded_heavyatoms = rsd.n_bonded_neighbor_all_res( at ) - rsd.type().number_bonded_hydrogens( at );
					if( ( bonded_heavyatoms + atom_hbonds.value()[ atid ] ) < satisfac_cut ){

						//TR << rsd.type().atom_name( at ) << " of res " << i << " has " << atom_sasa.value()[atid] << " sasa and " << cursasa << " combined sasa, and " << bonded_heavyatoms << " bonded heavyatoms, and " <<  atom_hbonds.value()[ atid ] << " hbonds, counts as buried unsatisfied." << std::endl;

						all_bur_unsat_polars_++;
						residue_bur_unsat_polars_[i]++;
						this_atom_bur_unsat = true;

						if( special_region_.find( i ) != special_region_.end() ) special_region_bur_unsat_polars_++;
					}
				}
			}
			atom_bur_unsat_.set( atid, this_atom_bur_unsat );
		}
	}


} //recompute

core::Size
BuriedUnsatisfiedPolarsCalculator::satisfaction_cutoff( std::string atom_type )
{

	//according to jk, buried hydroxyls are often seen making only one hydrogen bond. also, ether oxygens often are bad h-bond acceptors
	if( atom_type == "OH" ) return 2;

	//backbone oxygens also only have one h-bbond in most secondary structure elements
	else if (atom_type == "OCbb") return 2;

	else if( atom_type ==  "S")	return 2;

 //everything else we expect to have 3 bonded/h-bonded neighbours to count as satisfied
	else return 3;


}

} //namespace pose_metric_calculators
} //namespace toolbox
} //namespace protocols
