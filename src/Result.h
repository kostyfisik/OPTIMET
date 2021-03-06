// (C) University College London 2017
// This file is part of Optimet, licensed under the terms of the GNU Public License
//
// Optimet is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Optimet is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Optimet. If not, see <http://www.gnu.org/licenses/>.

#ifndef RESULT_H_
#define RESULT_H_

#include "CompoundIterator.h"
#include "Excitation.h"
#include "Geometry.h"
#include "OutputGrid.h"
#include "Spherical.h"
#include "SphericalP.h"

#include <complex>

namespace optimet {
/**
 * The Result class is used to provide post simulation
 * output functions including field profiles, absorbption
 * and extinction cross sections, and scattering
 * coefficients.
 */

class Result {
private:
  std::shared_ptr<Geometry> geometry;     /**< Pointer to the Geometry. */
  std::shared_ptr<Excitation> excitation; /**< Pointer to the Excitation. */
  std::complex<double> waveK;             /**< The std::complex wave number. */
  bool flagSH;                            /**< Specifies if handling Second Harmonic results. */
  Result *result_FF;                      /**< The Fundamental Frequency results vector. */
  //! Maximum nMax
  optimet::t_uint nMax;
public:
  Vector<t_complex> scatter_coef;   /**< The scattering coefficients. */
  Vector<t_complex> internal_coef;  /**< The internal coefficients. */
  Vector<t_complex> c_scatter_coef; /**< The cluster centered scattering coefficients. */

  /**
   * Initialization constructor for the Result class.
   * Fundamental Frequency version.
   * @param geometry_ the pointer to the geometry.
   * @param excitation_ the pointer to the excitation.
   */
  Result(std::shared_ptr<Geometry> geometry_, std::shared_ptr<Excitation> xcitation_);

  /**
   * Initialization constructor for the Result class.
   * Second Harmonic version.
   * @param geometry_ the pointer to the geometry.
   * @param excitation_ the pointer to the excitation.
   * @param result_FF_ the pointer to the Fundamental Frequency results.
   */
  Result(std::shared_ptr<Geometry> geometry_, std::shared_ptr<Excitation> excitation_,
         Result *result_FF_);

  /**
   * Default destructor for the Result class.
   */
  virtual ~Result(){};

  /**
   * Initialization method for the Result class.
   * Fundamental Frequency version.
   * @param geometry_ the pointer to the geometry.
   * @param excitation_ the pointer to the excitation.
   */
  void
  init(std::shared_ptr<Geometry> geometry_, std::shared_ptr<Excitation> excitation_);

  /**
   * Update method for the Result class.
   * @param geometry_ the pointer to the geometry.
   * @param excitation_ the pointer to the excitation.
   */
  void
  update(std::shared_ptr<Geometry> geometry_, std::shared_ptr<Excitation> excitation_);

  /**
   * Initialization constructor for the Result class.
   * Second Harmonic version.
   * @param geometry_ the pointer to the geometry.
   * @param excitation_ the pointer to the excitation.
   * @param result_FF_ the pointer to the Fundamental Frequency results.
   */
  void init(std::shared_ptr<Geometry> geometry_, std::shared_ptr<Excitation> excitation_,
            Result *result_FF_);

  /**
   * Returns the E field at a given point using the cluster centered
   * formulation.
   * @warning Testing method only. DO NOT USE IN PRODUCTION CODE!
   * @param R_ the position of the point.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   * @return the value of the E field
   */
  SphericalP<std::complex<double>> getEFieldC(Spherical<double> R_, bool projection_);

  /**
   * Returns the E and H fields at a given point.
   * @param R_ the coordinates of the point.
   * @param EField_ SphericalP vector that will store the E field.
   * @param HField_ SphericalP vector that will store the H field.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   */
  void getEHFields(Spherical<double> R_, SphericalP<std::complex<double>> &EField_,
                   SphericalP<std::complex<double>> &HField_, bool projection_) const;
  /**
   * Returns the E and H fields at a given point.
   * @param R_ the coordinates of the point.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   */
  Eigen::Matrix<t_complex, 3, 2> getEHFields(Spherical<double> R_, bool projection_ = false) const;
  template <class T>
  Eigen::Matrix<t_complex, 3, 2>
  getEHFields(Eigen::MatrixBase<T> const &R_, bool projection_ = false) const {
    return getEHFields(Spherical<double>::toSpherical(R_), projection_);
  }

  /**
   * Returns the E and H fields for a single harmonic and/or TE/TM component.
   * @param R_ the coordinates of the point.
   * @param EField_ SphericalP vector that will store the E field.
   * @param HField_ SphericalP vector that will store the H field.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   * @param p_ the harmonic to be used.
   * @param singleComponent_ return TE+TM (0), TE(1) or TM(2).
   */
  void getEHFieldsModal(Spherical<double> R_, SphericalP<std::complex<double>> &EField_,
                        SphericalP<std::complex<double>> &HField_, bool projection_,
                        CompoundIterator p_, int singleComponent_);

  /**
   * Center the scattering coefficients.
   * @warning Test method only! DO NOT USE FOR PRODUCTION CODE!
   */
  void centerScattering();

  /**
   * Returns the Extinction Cross Section.
   * @return the extinction cross section.
   */
  double getExtinctionCrossSection();

  /**
   * Returns the Absorption Cross Section.
   * @return the absorptions cross section.
   */
  double getAbsorptionCrossSection();

  /**
   * Populate a grid with E and H fields.
   * @param oEGrid_ the OutputGrid object for the E fields.
   * @param oHGrid_ the OutputGrid object for the H fields.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   * @return 0 if succesful, 1 otherwise.
   */
  int setFields(OutputGrid &oEGrid_, OutputGrid &oHGrid_, bool projection_);

  /**
   * Populate a grid with E and H fields for a single harmonic and/or TE/TM
   * component.
   * @param oEGrid_ the OutputGrid object for the E fields.
   * @param oHGrid_ the OutputGrid object for the H fields.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   * @param p_ the harmonic to be used.
   * @param singleComponent_ return TE+TM (0), TE(1) or TM(2).
   * @return 0 if succesful, 1 otherwise.
   */
  int setFieldsModal(OutputGrid &oEGrid_, OutputGrid &oHGrid_, bool projection_,
                     CompoundIterator p_, int singleComponent_);

  /**
   * Return the dominant harmonic.
   * @return the CompoundIterator corresponding to the dominant harmonic
   * (TE+TM).
   */
  CompoundIterator getDominant();

  /**
   * Returns the E and H fields at a given point using either the scattered or
   * internal field methods.
   * Used for continuity tests.
   * @param R_ the coordinates of the point.
   * @param EField_ SphericalP vector that will store the E field.
   * @param HField_ SphericalP vector that will store the H field.
   * @param projection_ defines spherical (1) or cartesian (0) projection.
   * @param inside_ uses the internal (1) or external (0) field calculations.
   */
  void getEHFieldsContCheck(Spherical<double> R_, SphericalP<std::complex<double>> &EField_,
                            SphericalP<std::complex<double>> &HField_, bool projection_,
                            int inside_);

  /**
   * Writes a set of files that check the field continuity around a particular
   * object.
   * @param objectIndex_ the object index for field continuity check.
   */
  void writeContinuityCheck(int objectIndex_);
};
}
#endif /* RESULT_H_ */
