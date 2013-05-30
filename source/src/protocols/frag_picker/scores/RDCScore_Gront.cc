// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite && is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/frag_picker/scores/RDCScore.cc
/// @brief
/// @author Dominik Gront (dgront@chem.uw.edu.pl)

#include <protocols/frag_picker/scores/RDCScore.hh>

// package headers
#include <protocols/frag_picker/FragmentCandidate.hh>
#include <protocols/frag_picker/scores/CachingScoringMethod.hh>
#include <protocols/frag_picker/scores/FragmentScoreMap.hh>
// AUTO-REMOVED #include <protocols/frag_picker/FragmentPicker.hh>

// AUTO-REMOVED #include <utility/vector1.hh>
#include <utility/io/izstream.hh>
#include <utility/excn/Exceptions.hh>

// option key includes
// AUTO-REMOVED #include <core/init/init.hh>
// AUTO-REMOVED #include <basic/options/option.hh>
// AUTO-REMOVED #include <basic/options/option_macros.hh>
// AUTO-REMOVED #include <basic/options/keys/OptionKeys.hh>
// AUTO-REMOVED #include <basic/options/keys/constraints.OptionKeys.gen.hh>

#include <core/scoring/ResidualDipolarCoupling.hh>
#include <basic/Tracer.hh>

//Auto Headers
#include <utility/options/keys/BooleanOptionKey.hh>


namespace protocols {
namespace frag_picker {
namespace scores {

#define ROTATE(a,i,j,k,l) g=a[i][j];h=a[k][l];a[i][j]=g-s*(h+g*tau); a[k][l]=h+s*(g-h*tau);

utility::vector1<std::string>& RDCScore::rdc_atoms() {

	rdc_atoms_.empty();
	rdc_atoms_.push_back("H");
	rdc_atoms_.push_back("H1");
	rdc_atoms_.push_back("N");
	rdc_atoms_.push_back("CA");
	rdc_atoms_.push_back("CB");

	return rdc_atoms_;
}

static basic::Tracer trRDCScore("fragment.picking.scores.RDCScore");

/// @param priority - the priority for this scoring method. The lower the priority, the later the score will be evaluated
/// Because a fragment may be discarded when a score is too low, the most accurate && meaningful scores should have the highest priority
/// @param lowest_acceptable_value - a fragment for which this score is below a certain threshold will be discarded
/// @param rdc_file_name - from this file RDC data will be obtained
/// @param query_size - the number of residues in the query sequence
RDCScore::RDCScore(Size priority, Real lowest_acceptable_value, bool use_lowest, Size query_size) :
	AtomBasedConstraintsScore(priority, lowest_acceptable_value, use_lowest, query_size,
			rdc_atoms(), "RDCScore") {
}

void RDCScore::do_caching(VallChunkOP chunk) {

	AtomBasedConstraintsScore::do_caching(chunk);
}

void RDCScore::clean_up() {
	AtomBasedConstraintsScore::clean_up();
}

void RDCScore::read_RDC_file(std::string const & filename, Size exp_id = 1) {

	utility::io::izstream infile(filename.c_str());
	std::string line;

	trRDCScore.Info << "Reading RDC file " << filename << std::endl;
	Size n = 0;
	while (getline(infile, line)) {
		std::istringstream line_stream(line);
		std::string atom1, atom2;
		Size res1, res2;
		Real Jdipolar;
		line_stream >> res1 >> atom1 >> res2 >> atom2 >> Jdipolar;

		if (atom1 == "HN")
			atom1 = "H"; //take care of typical NMR community notation
		if (atom2 == "HN")
			atom2 = "H";
		if (res1 == 1 && atom1 == "H")
			atom1 == "H1"; //or should it be ignored ?
		if (res2 == 1 && atom2 == "H")
			atom1 == "H1";
		if (line_stream.fail()) {
			trRDCScore.Error << "couldn't read line " << line
					<< " in rdc-file " << filename << "\n";
			throw(utility::excn::EXCN_BadInput(" invalid line " + line
					+ " in rdc-file " + filename));
		}

		Real weight(1.0);
		line_stream >> weight;
		if (line_stream.fail()) {
			trRDCScore.Debug << " set weight for RDC " << res1 << " to 1.0 "
					<< std::endl;
			weight = 1.0;
		}
		rdc_data_.push_back(core::scoring::RDC(res1, atom1, res2, atom2,
				Jdipolar, weight, exp_id - 1 /*C-style counting*/));
		n++;
		trRDCScore.Debug << rdc_data_[n];
	}
	trRDCScore.Debug << "\n" << rdc_data_.size()
			<< " data points acquired from " << filename << std::endl;
}

int RDCScore::m_inv_gen(Real m[5][5], int n, Real minv[5][5]) {
	Real md[5][5], v[5][5];
	Real eig[5];

	Real tol, s;
	int nzero, i, j, k, nrot;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			md[i][j] = m[i][j];

	tol = 0;
	for (i = 0; i < n; i++)
		tol += fabs(md[i][i]);
	tol = 1e-6 * tol / n;

	jacobi(md, eig, v, &nrot);

	nzero = 0;
	for (i = 0; i < n; i++)
		if (fabs(eig[i]) < tol) {
			eig[i] = 0;
			nzero++;
		} else
			eig[i] = 1.0 / eig[i];

	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++) {
			s = 0;
			for (k = 0; k < n; k++)
				s += eig[k] * v[i][k] * v[j][k];
			minv[i][j] = s;
		}

	return nzero;
}

void RDCScore::jacobi(Real a[5][5], Real d[], Real v[5][5], int *nrot) {
	int j, i;
	int iq, ip;
	Real tresh, theta, tau, t, sm, s, h, g, c;
	Real b[5];
	Real z[5];
	int const n(5);
	for (ip = 0; ip < n; ip++) {
		for (iq = 0; iq < n; iq++)
			v[ip][iq] = 0.0;
		v[ip][ip] = 1.0;
	}
	for (ip = 0; ip < n; ip++) {
		b[ip] = d[ip] = a[ip][ip];
		z[ip] = 0.0;
	}
	*nrot = 0;
	for (i = 1; i <= 50; i++) {
		sm = 0.0;
		for (ip = 0; ip < n - 1; ip++) {
			for (iq = ip + 1; iq < n; iq++)
				sm += fabs(a[ip][iq]);
		}
		if (sm == 0.0) {
			return;
		}
		if (i < 4) //first 3 iterations
			tresh = 0.2 * sm / (n * n);
		else
			tresh = 0.0;
		for (ip = 0; ip < n - 1; ip++) {
			for (iq = ip + 1; iq < n; iq++) {
				g = 100.0 * fabs(a[ip][iq]);
				if (i > 4 && fabs(d[ip]) + g == fabs(d[ip]) && fabs(d[iq]) + g
						== fabs(d[iq]))
					a[ip][iq] = 0.0;
				else if (fabs(a[ip][iq]) > tresh) {
					h = d[iq] - d[ip];
					if (fabs(h) + g == fabs(h))
						t = (a[ip][iq]) / h;
					else {
						theta = 0.5 * h / (a[ip][iq]);
						t = 1.0 / (fabs(theta) + sqrt(1.0 + theta * theta));
						if (theta < 0.0)
							t = -t;
					}
					c = 1.0 / sqrt(1 + t * t);
					s = t * c;
					tau = s / (1.0 + c);
					h = t * a[ip][iq];
					z[ip] -= h;
					z[iq] += h;
					d[ip] -= h;
					d[iq] += h;
					a[ip][iq] = 0.0;
					for (j = 0; j < ip; j++) {
						ROTATE (a,j,ip,j,iq)
					}
					for (j = ip + 1; j < iq; j++) {
						ROTATE(a,ip,j,j,iq)
					}
					for (j = iq + 1; j < n; j++) {
						ROTATE(a,ip,j,iq,j)
					}
					for (j = 0; j < n; j++) {
						ROTATE(v,j,ip,j,iq)
					}
					++(*nrot);
				}
			}
		}
		for (ip = 0; ip < n; ip++) {
			b[ip] += z[ip];
			d[ip] = b[ip];
			z[ip] = 0.0;
		}
	}
	//probably different type of Exception is better suited
	throw(utility::excn::EXCN_BadInput(
			" too many iterations in Jacobi when compute RDC tensor"));
}

bool RDCScore::score(FragmentCandidateOP f, FragmentScoreMapOP empty_map) {

	Real total_score = 0;
	Size frag_len = f->get_length();
	empty_map->set_score_component(total_score, id_ + frag_len - frag_len); // just to prevent a compiler from complaining bout unused frag_len

	return true;
}

/*void RDCScore::evaluate_score(core::pose::PoseOP pose, utility::vector1<
 core::scoring::RDC> & data) {

 bool const
 correct_NH(
 basic::options::option[basic::options::OptionKeys::rdc::correct_NH_length]);
 bool const
 bReduced(
 basic::options::option[basic::options::OptionKeys::rdc::reduced_couplings]);

 const Size nrow = data.size();
 Real D_[1000][5]; //rvec5 x nrows
 Real rhs_[5]; //rvec5 x nrows
 Real S_[3][3]; // 3 x 3 x nex
 Real T_[5][5]; // 5 x 5 x nex

 trace.Debug << "Evaluating RDC score based on " << data.size()
 << " data points" << std::endl;

 utility::vector1<core::scoring::RDC>::const_iterator it;
 Size d;
 d = 0;
 for (it = data.begin(); it != data.end(); ++it) {

 numeric::xyzVector<Real> r(
 pose->residue(it->res1()).atom(it->atom1()).xyz()
 - pose->residue(it->res2()).atom(it->atom2()).xyz());

 core::Real r2 = r.norm_squared();
 if (it->type() == RDC::RDC_TYPE_NH && correct_NH)
 r2 = 1.04 * 1.04;

 core::Real invr = 1.0 / sqrt(r2);
 core::Real pfac = it->Dconst() * invr * invr;
 pfac *= invr * invr * invr;
 D_[d][0] = 3* pfac * (2* r [0] * r[0] + r[1] * r[1] - r2);
 D_[d][1] = 3* pfac * (2* r [0] * r[1]);
 D_[d][2] = 3* pfac * (2* r [0] * r[2]);
 D_[d][3] = 3* pfac * (2* r [1] * r[1] + r[0] * r[0] - r2);
 D_[d][4] = 3* pfac * (2* r [1] * r[2]);
 d++;
 } //~ End of atoms

 // Calculate the order tensor S for each experiment via optimization
 for (Size i = 0; i < 5; i++) {
 rhs_[i] = 0;
 for (Size j = 0; j <= i; j++) {
 T_[i][j] = T_[j][i] = 0;
 }
 }

 for (core::Size d = 0; d < data.size(); d++) {
 core::Real weight = data[d + 1].weight(); //force constant
 core::Real obs = data[d + 1].Jdipolar();
 // Calculate the vector rhs && half the matrix T for the 5 equations
 for (Size i = 0; i < 5; i++) {
 rhs_[i] += D_[d][i] * obs * weight;
 for (Size j = 0; j <= i; j++)
 T_[i][j] += D_[d][i] * D_[d][j] * weight;
 }
 }

 // Now we have all the data we can calculate S
 // Correct corrfac && copy one half of T to the other half
 for (Size i = 0; i < 5; i++) {
 for (Size j = 0; j < i; j++) {
 T_[j][i] = T_[i][j];
 }
 }
 try {
 m_inv_gen(T_, 5, T_);
 } catch (utility::excn::EXCN_BadInput &excn) {
 if (trace.Debug) {
 pose->dump_pdb("failed_jacobi.pdb");
 }
 throw excn;
 }
 // Calculate the orientation tensor S for this experiment
 S_[0][0] = 0;
 S_[0][1] = 0;
 S_[0][2] = 0;
 S_[1][1] = 0;
 S_[1][2] = 0;
 for (Size i = 0; i < 5; i++) {
 S_[0][0] += 1.5 * T_[0][i] * rhs_[i];
 S_[0][1] += 1.5 * T_[1][i] * rhs_[i];
 S_[0][2] += 1.5 * T_[2][i] * rhs_[i];
 S_[1][1] += 1.5 * T_[3][i] * rhs_[i];
 S_[1][2] += 1.5 * T_[4][i] * rhs_[i];
 }
 S_[1][0] = S_[0][1];
 S_[2][0] = S_[0][2];
 S_[2][1] = S_[1][2];
 S_[2][2] = -S_[0][0] - S_[1][1];
 Real Smax = sqrt(sqr(S_[0][0]) + sqr(S_[0][1]) + sqr(S_[0][2]) + sqr(
 S_[1][1]) + sqr(S_[1][2]));

 trace.Debug << "Smax( " << 1 << " ): " << Smax << std::endl;

 Real wsv2 = 0;
 Real sw = 0;
 Real two_thr = 2.0 / 3.0;
 Real vtot = 0;
 Real Q = 0;
 Real Qnorm = 0;
 core::Real const Rohl2Hess(2.5);

 Size irow(0);
 for (utility::vector1<core::scoring::RDC>::iterator it = data.begin(); it
 != data.end(); ++it) {
 ++irow;
 Size const d(irow - 1);

 //for( Size d = 0; d<nrow; d++ ) {
 Size ex = it->expid(); //exp_id const 1 ... fix later

 Real computed_coupling = it->Jdipolar_computed_ = two_thr * (S_[0][0]
 * D_[d][0] + S_[0][1] * D_[d][1] + S_[0][2] * D_[d][2]
 + S_[1][1] * D_[d][3] + S_[1][2] * D_[d][4]);

 Size const power(3); //this will be 0 for CSA see above
 RDC & rdc = *it;
 numeric::xyzVector<Real> r(
 pose->residue(rdc.res1()).atom(rdc.atom1()).xyz()
 - pose->residue(rdc.res2()).atom(rdc.atom2()).xyz());
 core::Real r2 = r.norm_squared();
 core::Real invr = 1.0 / sqrt(r2);
 core::Real invr2 = sqr(invr);
 core::Real pfac = rdc.Dconst() * invr2 * invr2 * invr * Rohl2Hess
 / (Smax * Smax) / data.size();

 core::Real const pfac_NH = 6.088 / 1.05 * Rohl2Hess / (Smax * Smax)
 / data.size();
 Real obs = it->Jdipolar();
 Real dev = computed_coupling - obs;
 if (bReduced) {
 trace.Trace << "reducing coupling for " << rdc << " dev: " << dev
 << " pfac: " << pfac << " pfac_NH " << pfac_NH;
 dev *= pfac_NH / pfac;
 obs *= pfac_NH / pfac;
 trace.Trace << " new dev: " << dev << std::endl;
 }

 Real Sr[3];
 Real rgmx[3];
 rgmx[0] = r[0];
 rgmx[1] = r[1];
 rgmx[2] = r[2];
 mvmul(S_, rgmx, Sr);

 if (bReduced)
 pfac = pfac_NH;
 for (Size i = 0; i < 3; i++) {
 rdc.fij_[i] = -pfac * dev * (4* Sr [i] - 2* (2 + power ) * invr2 *
 iprod (Sr ,rgmx) * rgmx[ i]);
 }

 Real weight = it->weight();
 vtot += 0.5*sqr( dev )/( Smax * Smax );
 wsv2 += weight*sqr(dev);
 sw += weight;
 Q += sqr( dev );
 Qnorm += sqr( obs );
 }
 Real R_ = sqrt(Q / Qnorm / 2);
 Real rmsd_ = sqrt(wsv2 / sw);

 trace.Debug<< Rohl2Hess*vtot/data.size()<<std::endl;
 }*/

}
} // frag_picker
} // protocols
