// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/task_operations/RestrictByCalculatorsOperation.cc
/// @brief  A class that applies arbitrary calculators (whose calculations return std::set< core::Size >) to restrict a PackerTask
/// @author Steven Lewis smlewi@gmail.com

// Unit Headers
#include <protocols/task_operations/RestrictByCalculatorsOperation.hh>
#include <protocols/task_operations/RestrictByCalculatorsOperationCreator.hh>

// Project Headers
#include <core/pose/Pose.hh>

#include <core/pack/task/PackerTask.hh>


// Utility Headers
#include <core/types.hh>
#include <utility>
#include <basic/Tracer.hh>

#include <utility/vector1.hh>
#include <utility/tag/XMLSchemaGeneration.fwd.hh>
#include <core/pack/task/operation/task_op_schemas.hh>


// C++ Headers

static basic::Tracer TR( "protocols.TaskOperations.RestrictByCalculatorsOperation" );

namespace protocols {
namespace task_operations {

using namespace core::pack::task::operation;
using namespace utility::tag;

core::pack::task::operation::TaskOperationOP
RestrictByCalculatorsOperationCreator::create_task_operation() const
{
	return utility::pointer::make_shared< RestrictByCalculatorsOperation >();
}

void RestrictByCalculatorsOperationCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	RestrictByCalculatorsOperation::provide_xml_schema( xsd );
}

std::string RestrictByCalculatorsOperationCreator::keyname() const
{
	return RestrictByCalculatorsOperation::keyname();
}

RestrictByCalculatorsOperation::RestrictByCalculatorsOperation() = default;

RestrictByCalculatorsOperation::RestrictByCalculatorsOperation(utility::vector1< calc_calcn > const & calcs_and_calcns)
: parent(), calcs_and_calcns_(calcs_and_calcns)
{
	//I suppose you could reasonably create this object BEFORE the calculator was generated/registered
	//  for( core::Size i(1); i <= calcs_and_calcns_.size(); ++i){
	//   if( !core::pose::metrics::CalculatorFactory::Instance().check_calculator_exists( calcs_and_calcns_[i].first ) ){
	//    utility_exit_with_message("In RestrictByCalculatorsOperation, calculator " + calcs_and_calcns_[i].first + " does not exist.");
	//   }
	//  }
}

RestrictByCalculatorsOperation::~RestrictByCalculatorsOperation() = default;

/// @details be warned if you use clone that you'll not get new calculators
core::pack::task::operation::TaskOperationOP RestrictByCalculatorsOperation::clone() const
{
	return utility::pointer::make_shared< RestrictByCalculatorsOperation >( *this );
}

void
RestrictByCalculatorsOperation::apply( core::pose::Pose const & pose, core::pack::task::PackerTask & task ) const
{
	//vector for filling packertask
	utility::vector1_bool repack(pose.size(), false);

	for ( core::Size i(1); i <= calcs_and_calcns_.size(); ++i ) {
		run_calculator(pose, calcs_and_calcns_[i].first, calcs_and_calcns_[i].second, repack);
	}

	task.restrict_to_residues(repack);
	return;
}

// AMW: No parse_tag, so nothing to add...
void RestrictByCalculatorsOperation::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	task_op_schema_empty( xsd, keyname(), "XRW TO DO" );
}

} //namespace protocols
} //namespace task_operations
