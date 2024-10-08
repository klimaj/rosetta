// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/select/residue_selector/PrimarySequenceNeighborhoodSelector.hh
/// @brief  The PrimarySequenceNeighborhoodSelector selects residues using a string containing residue names
/// @author Tom Linsky (tlinsky@uw.edu))

// Unit headers
#include <core/select/residue_selector/PrimarySequenceNeighborhoodSelector.hh>
#include <core/select/residue_selector/PrimarySequenceNeighborhoodSelectorCreator.hh>

// Protocol Headers

// Core headers
#include <core/select/residue_selector/ResidueRanges.hh>
#include <core/select/residue_selector/ResidueSelectorFactory.hh>
#include <core/select/residue_selector/util.hh>
#include <core/pose/Pose.hh>
#include <core/pose/variant_util.hh>

// Basic Headers
#include <basic/datacache/DataMap.fwd.hh>
#include <basic/Tracer.hh>

// Utility Headers
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>

// C++ headers

static basic::Tracer TR( "core.select.residue_selector.PrimarySequenceNeighborhoodSelector" );

namespace core {
namespace select {
namespace residue_selector {

PrimarySequenceNeighborhoodSelector::PrimarySequenceNeighborhoodSelector() :
	lower_residues_( 1 ),
	upper_residues_( 1 ),
	selector_(),
	cross_chain_boundaries_( false )
{
}

/// @brief Clone operator.
/// @details Copy this object and return an owning pointer to the new object.
ResidueSelectorOP PrimarySequenceNeighborhoodSelector::clone() const { return utility::pointer::make_shared< PrimarySequenceNeighborhoodSelector >(*this); }

PrimarySequenceNeighborhoodSelector::PrimarySequenceNeighborhoodSelector(
	core::Size const lower_residues,
	core::Size const upper_residues,
	core::select::residue_selector::ResidueSelectorCOP const selector,
	bool cross_chain_boundaries ) :
	lower_residues_( lower_residues ),
	upper_residues_( upper_residues ),
	selector_( selector ),
	cross_chain_boundaries_( cross_chain_boundaries )
{
}


PrimarySequenceNeighborhoodSelector::~PrimarySequenceNeighborhoodSelector() = default;

ResidueSubset
PrimarySequenceNeighborhoodSelector::apply( core::pose::Pose const & pose ) const
{
	runtime_assert( selector_ );
	ResidueRanges const ranges( selector_->apply( pose ) );

	core::select::residue_selector::ResidueSubset retval( pose.size(), false );
	TR << "Intervals: [";
	for ( auto const & range : ranges ) {
		TR << " " << range.start() << "->" << range.stop() << ",";
		Size start = range.start();
		Size end = range.stop();
		Size count = 0;
		while ( ( count < lower_residues_ ) && ( start > 1 ) && ! ( pose::is_lower_terminus( pose, start ) && ! cross_chain_boundaries_ ) ) {
			++count;
			--start;
		}
		TR.Debug << count << " residues added to lower terminus" << std::endl;
		count = 0;
		while ( ( count < upper_residues_ ) && ( end < pose.size() ) && ! ( pose::is_upper_terminus( pose, end ) && ! cross_chain_boundaries_ ) ) {
			++count;
			++end;
		}
		TR.Debug << count << " residues added to upper terminus" << std::endl;
		for ( Size r=start; r<=end; ++r ) {
			retval[r] = true;
		}
	}
	TR << "]" << std::endl;
	return retval;
}

void
PrimarySequenceNeighborhoodSelector::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data )
{
	if ( tag->hasOption( "lower" ) ) {
		set_lower_residues( tag->getOption< Size >( "lower" ) );
	}

	if ( tag->hasOption( "upper" ) ) {
		set_upper_residues( tag->getOption< Size >( "upper" ) );
	}
	if ( tag->hasOption( "cross_chain_boundaries" ) ) {
		set_cross_chain_boundaries( tag->getOption< bool >( "cross_chain_boundaries" ) );
	}

	if ( tag->hasOption( "selector" ) ) {
		std::string const selectorname = tag->getOption< std::string >( "selector" );
		try {
			set_selector( core::select::residue_selector::get_residue_selector(selectorname, data));
		} catch ( utility::excn::Exception & e ) {
			std::stringstream error_msg;
			error_msg << "Failed to find ResidueSelector named '" << selectorname << "' from the Datamap from DisulfidizeMover.\n";
			error_msg << e.msg();
			throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  error_msg.str() );
		}
		debug_assert( selector_ );
		TR << "Using residue selector " << selectorname << std::endl;
	} else if ( tag->getTags().size() ) {
		for ( auto const & t : tag->getTags() ) {
			ResidueSelectorCOP rs = ResidueSelectorFactory::get_instance()->new_residue_selector(
				t->getName(), t, data );
			if ( rs ) {
				debug_assert( !selector_ );
				set_selector( rs );
			}
		}
	} else {
		std::stringstream ss;
		ss << "PrimarySequenceNeighborhood requires a selector to be set either through the \"selector\" option or by placing a residue selector as a subtag." << std::endl;
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  ss.str() );
	}
	debug_assert( selector_ );
}

void
PrimarySequenceNeighborhoodSelector::set_selector(
	core::select::residue_selector::ResidueSelectorCOP const selector )
{
	selector_ = selector;
}

void
PrimarySequenceNeighborhoodSelector::set_lower_residues( core::Size const nres )
{
	lower_residues_ = nres;
}

void
PrimarySequenceNeighborhoodSelector::set_upper_residues( core::Size const nres )
{
	upper_residues_ = nres;
}

void
PrimarySequenceNeighborhoodSelector::set_cross_chain_boundaries( bool cross )
{
	cross_chain_boundaries_ = cross;
}

std::string
PrimarySequenceNeighborhoodSelector::get_name() const
{
	return PrimarySequenceNeighborhoodSelector::class_name();
}

std::string
PrimarySequenceNeighborhoodSelector::class_name()
{
	return "PrimarySequenceNeighborhood";
}

void
PrimarySequenceNeighborhoodSelector::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attributes;
	attributes_for_parse_residue_selector(attributes, "selector");
	attributes
		+ XMLSchemaAttribute( "lower", xsct_non_negative_integer , "XRW TO DO" )
		+ XMLSchemaAttribute( "upper", xsct_non_negative_integer , "XRW TO DO" )
		+ XMLSchemaAttribute( "cross_chain_boundaries", xsct_rosetta_bool, "Allow the selector to cross chain boundaries? By default false.");
	xsd_type_definition_w_attributes_and_optional_subselector( xsd, class_name(),"XRW TO DO", attributes );
}

ResidueSelectorOP
PrimarySequenceNeighborhoodSelectorCreator::create_residue_selector() const {
	return utility::pointer::make_shared< PrimarySequenceNeighborhoodSelector >();
}

std::string
PrimarySequenceNeighborhoodSelectorCreator::keyname() const {
	return PrimarySequenceNeighborhoodSelector::class_name();
}

void
PrimarySequenceNeighborhoodSelectorCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	PrimarySequenceNeighborhoodSelector::provide_xml_schema( xsd );
}

} //namespace residue_selector
} //namespace select
} //namespace core
