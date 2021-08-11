// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/pose_sewing/simple_metrics/BlockwiseShapeCompMetric.hh
/// @brief composite metric returning shape complementarities that pass per block
/// @author frankdt (frankdt@email.unc.edu)
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#ifndef INCLUDED_protocols_pose_sewing_simple_metrics_BlockwiseShapeCompMetric_HH
#define INCLUDED_protocols_pose_sewing_simple_metrics_BlockwiseShapeCompMetric_HH

#include <protocols/pose_sewing/simple_metrics/BlockwiseShapeCompMetric.fwd.hh>
#include <core/simple_metrics/CompositeRealMetric.hh>

// Core headers
#include <core/types.hh>

// Utility headers
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

// C++ headers
#include <map>
#include <core/select/residue_selector/BlockSelector.hh>

#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION

namespace protocols {
namespace pose_sewing {
namespace simple_metrics {

///@brief composite metric returning shape complementarities that pass per block
class BlockwiseShapeCompMetric : public core::simple_metrics::CompositeRealMetric{

public:

	/////////////////////
	/// Constructors  ///
	/////////////////////

	/// @brief Default constructor
	BlockwiseShapeCompMetric();

	/// @brief Copy constructor (not needed unless you need deep copies)
	BlockwiseShapeCompMetric( BlockwiseShapeCompMetric const & src );

	/// @brief Destructor (important for properly forward-declaring smart-pointer members)
	~BlockwiseShapeCompMetric() override;


public:

	/////////////////////
	/// Metric Methods ///
	/////////////////////

	///Defined in RealMetric:
	///
	/// @brief Calculate the metric and add it to the pose as a score.
	///           labeled as prefix+metric+suffix.
	///
	/// @details Score is added through setExtraScorePose and is output
	///            into the score tables/file at pose output.
	//void
	//apply( pose::Pose & pose, prefix="", suffix="" ) override;

	///@brief Calculate the metric.
	std::map< std::string, core::Real >
	calculate( core::pose::Pose const & pose ) const override;

	void
	set_selector(core::select::residue_selector::ResidueSelectorCOP selector);

	void
	set_filtered_sc(core::Real filtered_sc);

	void
	set_filtered_area(core::Real filtered_area);

	void
	set_filtered_d_median(core::Real filtered_d_median);

	void
	set_scale_by_length(bool in);

public:

	///@brief Name of the class
	std::string
	name() const override;

	///@brief Name of the class for creator.
	static
	std::string
	name_static();

	///@brief Name of the metric
	std::string
	metric() const override;

	///@brief Get the submetric names that this Metric will calculate
	utility::vector1< std::string >
	get_metric_names() const override;

public:

	/// @brief called by parse_my_tag -- should not be used directly
	void
	parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data ) override;

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	core::simple_metrics::SimpleMetricOP
	clone() const override;

private:
	core::select::residue_selector::ResidueSelectorCOP selector_;
	core::Real filtered_sc_ = 0.5;
	core::Real filtered_area_ = 0;
	core::Real filtered_d_median_ = 0;
	bool scale_by_length_ = false;



};

} //simple_metrics
} //pose_sewing
} //protocols


#endif //protocols_pose_sewing_simple_metrics_BlockwiseShapeCompMetric_HH





