// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/filters/AtomicDistanceFilter.cc
/// @brief Filter for looking at specific atom distances
/// @author Rocco Moretti (rmoretti@uw.edu)

#include <protocols/simple_filters/AtomicDistanceFilter.hh>
#include <protocols/simple_filters/AtomicDistanceFilterCreator.hh>

// Project Headers
#include <core/types.hh>
#include <core/pose/Pose.hh>

//parsing
#include <utility/tag/Tag.hh>
#include <core/conformation/Residue.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <core/pose/ResidueIndexDescription.hh>
#include <core/pose/selection.hh>
#include <basic/Tracer.hh>
#include <core/select/residue_selector/ResidueSelector.hh>
#include <core/select/residue_selector/util.hh>

#include <utility/excn/Exceptions.hh>
#include <utility/vector1.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/filters/filter_schemas.hh>

namespace protocols {
namespace simple_filters {

static basic::Tracer TR( "protocols.filters.AtomicDistanceFilter" );

namespace {//helper functions

///@brief figure out which of description and selector is active and return the resid that it selects
///@details not prototyped in the .hh file because it serves no purpose outside of this class
core::Size
determine_resid(
	core::pose::ResidueIndexDescriptionCOP const & description,
	core::select::residue_selector::ResidueSelectorCOP const & selector,
	core::pose::Pose const & pose
) {
	if ( description != nullptr ) { //ResidueIndexDescription option
		runtime_assert( selector == nullptr );
		return description->resolve_index( pose );

	} else { //ResidueSelector option
		runtime_assert( selector != nullptr );
		utility::vector1< bool > const sele = selector->apply( pose );
		utility::vector1< core::Size > const positions =
			core::select::residue_selector::selection_positions( sele );
		if ( positions.size() != 1 ) {
			utility_exit_with_message( "AtomicDistanceFilter expected a residue selector to select exactly one residues. Instead, it selected " + std::to_string( positions.size() ) );
		}
		return positions[ 1 ];
	}
}
}//anonymous namespace

/// @brief default ctor
AtomicDistanceFilter::AtomicDistanceFilter() :
	parent( "AtomicDistance" )
{}

/// @brief
AtomicDistanceFilter::AtomicDistanceFilter( core::Size const res1, core::Size const res2, std::string const & atom_desig1, std::string const & atom_desig2, bool as_type1, bool as_type2, core::Real distance) :
	parent( "AtomicDistance" ),
	residue1_( core::pose::make_rid_posenum( res1 ) ),
	residue2_( core::pose::make_rid_posenum( res2 ) ),
	atomdesg1_( atom_desig1 ),
	atomdesg2_( atom_desig2 ),
	astype1_( as_type1 ),
	astype2_( as_type2 ),
	distance_( distance )
{}

/// @return Whether the atom pair is within the cutoff
bool AtomicDistanceFilter::apply(core::pose::Pose const & pose ) const
{
	core::Real const dist( compute( pose ) );
	report( TR.Debug, pose );
	if ( dist <= distance_ ) return true;
	return false;
}

core::Real
AtomicDistanceFilter::compute( core::pose::Pose const & pose ) const
{
	using namespace core::conformation;

	core::Real nearest_distance( 999999 );

	Residue const & res1 = pose.residue( determine_resid( residue1_, selector1_, pose ) );
	Residue const & res2 = pose.residue( determine_resid( residue2_, selector2_, pose ) );

	core::Size a1primet(1), a1end(res1.natoms());
	if ( ! astype1_ ) { // If given by name, look only at the single atom
		if ( ! res1.type().has(atomdesg1_) ) {
			TR.Warning << "Residue " << res1.seqpos() <<" of type "<<res1.type().name()<<" does not have atom with name "<<atomdesg1_<<std::endl;
			return nearest_distance;
		}
		a1primet = a1end = res1.atom_index(atomdesg1_);
	}
	core::Size a2primet(1), a2end(res2.natoms());
	if ( ! astype2_ ) { // If given by name, look only at the single atom
		if ( ! res2.type().has(atomdesg2_) ) {
			TR.Warning << "Residue " << res2.seqpos() <<" of type "<<res2.type().name()<<" does not have atom with name "<<atomdesg2_<<std::endl;
			return nearest_distance;
		}
		a2primet = a2end = res2.atom_index(atomdesg2_);
	}

	bool found1(false), found2(false);
	for ( core::Size ii(a1primet); ii <= a1end; ++ii ) {
		if ( !astype1_ || res1.atom_type(ii).name() == atomdesg1_ ) {
			found1 = true;
			for ( core::Size jj(a2primet); jj <= a2end; ++jj ) {
				if ( !astype2_ || res2.atom_type(jj).name() == atomdesg2_ ) {
					found2 = true;
					core::Real const dist( res1.atom(ii).xyz().distance( res2.atom(jj).xyz() ) );
					if ( dist < nearest_distance ) {
						nearest_distance = dist;
					}
				}
			}
		}
	}

	if ( ! found1 ) {
		TR.Warning << "Residue " << res1.seqpos() <<" of type "<<res1.type().name()<<" does not have atom with "<<(astype1_?"type ":"name ")<<atomdesg1_<<std::endl;
	} else if ( ! found2 ) { // elseif because the inner loop doesn't run if the outer loop doesn't trip. (if found1 is false, found2 is always false)
		TR.Warning << "Residue " << res2.seqpos() <<" of type "<<res2.type().name()<<" does not have atom with "<<(astype2_?"type ":"name ")<<atomdesg2_<<std::endl;
	}
	return( nearest_distance );
}

core::Real
AtomicDistanceFilter::report_sm( core::pose::Pose const & pose ) const
{
	return compute( pose );
}

void AtomicDistanceFilter::report( std::ostream & out, core::pose::Pose const & pose ) const
{
	core::Real const dist( compute( pose ) );
	out<<"Minimal distance between Residue 1 atom "<<(astype1_?"type ":"name ")<<atomdesg1_<<" and Residue 2 atom "<<(astype2_?"type ":"name ")<<atomdesg2_<<" is "<<dist<<std::endl;
}

void AtomicDistanceFilter::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data
)
{
	distance_ = tag->getOption< core::Real >( "distance", 4.0 );

	if ( tag->hasOption( "residue1" ) ) {
		std::string const res1( tag->getOption< std::string >( "residue1" ) );
		residue1_ = core::pose::parse_resnum( res1 );
		selector1_ = nullptr;
		runtime_assert_msg( ! tag->hasOption( "res1_selector" ), "Please only use one of 'res1_selector' or 'residue1', not both" );
	} else if ( tag->hasOption( "res1_selector" ) ) {
		selector1_ = protocols::rosetta_scripts::parse_residue_selector( tag, data, "res1_selector" );
		residue1_ = nullptr;
	} else {
		utility_exit_with_message( "Please provide either residue1 or res1_selector to AtomicDistanceFilter" );
	}

	if ( tag->hasOption( "residue2" ) ) {
		std::string const res2( tag->getOption< std::string >( "residue2" ) );
		residue2_ = core::pose::parse_resnum( res2 );
		selector2_ = nullptr;
		runtime_assert_msg( ! tag->hasOption( "res2_selector" ), "Please only use one of 'res2_selector' or 'residue2', not both" );
	} else if ( tag->hasOption( "res2_selector" ) ) {
		selector2_ = protocols::rosetta_scripts::parse_residue_selector( tag, data, "res2_selector" );
		residue2_ = nullptr;
	} else {
		utility_exit_with_message( "Please provide either residue2 or res2_selector to AtomicDistanceFilter" );
	}


	if ( tag->hasOption( "atomtype1" ) ) {
		if ( tag->hasOption( "atomname1" ) ) {
			throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "Can't set both atomname1 and atomtype1. Check xml file");
		}
		atomdesg1_ = tag->getOption< std::string >( "atomtype1" );
		astype1_ = true;
	} else {
		atomdesg1_ = tag->getOption< std::string >( "atomname1", "CB" );
		astype1_ = false;
	}

	if ( tag->hasOption( "atomtype2" ) ) {
		if ( tag->hasOption( "atomname2" ) ) {
			throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "Can't set both atomname2 and atomtype2. Check xml file");
		}
		atomdesg2_ = tag->getOption< std::string >( "atomtype2" );
		astype2_ = true;
	} else {
		atomdesg2_ = tag->getOption< std::string >( "atomname2", "CB" );
		astype2_ = false;
	}

	TR<<"AtomicDistance filter between Residue 1 atom "<<(astype1_?"type ":"name ")<<atomdesg1_<<" and Residue 2 atom "<<(astype2_?"type ":"name ")<<atomdesg2_<<" with distance cutoff of "<<distance_<<std::endl;
}



std::string AtomicDistanceFilter::name() const {
	return class_name();
}

std::string AtomicDistanceFilter::class_name() {
	return "AtomicDistance";
}

void AtomicDistanceFilter::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	attlist + XMLSchemaAttribute::attribute_w_default( "distance", xsct_real, "Distance threshold between atoms in question", "4.0" )
		+ XMLSchemaAttribute( "atomtype1", xs_string, "Type desired for first atom" )
		+ XMLSchemaAttribute( "atomtype2", xs_string, "Type of second atom" )
		+ XMLSchemaAttribute::attribute_w_default( "atomname1", xs_string, "Name desired for first atom", "CB" )
		+ XMLSchemaAttribute::attribute_w_default( "atomname2", xs_string, "Name of second atom", "CB" );

	core::pose::attributes_for_parse_resnum( attlist, "residue1", "First residue" );
	core::pose::attributes_for_parse_resnum( attlist, "residue2", "Second residue" );

	core::select::residue_selector::attributes_for_parse_residue_selector(
		attlist, "res1_selector",
		"Alternative to using the residue1 option. This residue selector must select exactly one residue!" );

	core::select::residue_selector::attributes_for_parse_residue_selector(
		attlist, "res2_selector",
		"Alternative to using the residue2 option. This residue selector must select exactly one residue!" );


	protocols::filters::xsd_type_definition_w_attributes( xsd, class_name(), "Filters on the distance between two specific atoms or the minimal distance between atoms of two specific types, on one or two residues.", attlist );
}

std::string AtomicDistanceFilterCreator::keyname() const {
	return AtomicDistanceFilter::class_name();
}

protocols::filters::FilterOP
AtomicDistanceFilterCreator::create_filter() const {
	return utility::pointer::make_shared< AtomicDistanceFilter >();
}

void AtomicDistanceFilterCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	AtomicDistanceFilter::provide_xml_schema( xsd );
}



} // filters
} // protocols
