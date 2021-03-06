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

#include "Coupling.h"
#include "Geometry.h"

#include "Algebra.h"
#include "Bessel.h"
#include "CompoundIterator.h"
#include "HarmonicsIterator.h"
#include "Symbol.h"
#include "Tools.h"
#include "Types.h"
#include "constants.h"

#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

Geometry::~Geometry() {}
Geometry::Geometry() {}

void Geometry::pushObject(Scatterer const &object_) {
  for(auto const &obj : objects)
    if(Tools::findDistance(obj.vR, object_.vR) <= (object_.radius + obj.radius)) {
      std::ostringstream sstr;
      sstr << "The sphere at (" << Tools::toCartesian(object_.vR).x << ", "
           << Tools::toCartesian(object_.vR).y << ", " << Tools::toCartesian(object_.vR).z << ") "
           << "overlaps with the one at (" << Tools::toCartesian(obj.vR).x << ", "
           << Tools::toCartesian(obj.vR).y << ", " << Tools::toCartesian(obj.vR).z << "), "
           << "with radii " << object_.radius << " and " << obj.radius;
      throw std::runtime_error(sstr.str());
    }
  objects.emplace_back(object_);
}

bool Geometry::is_valid() const {
  using namespace optimet;
  if(objects.size() == 0)
    return false;
  for(t_uint i(0); i < objects.size(); ++i)
    for(t_uint j(0); j < objects.size(); ++j)
      if(Tools::findDistance(objects[i].vR, objects[j].vR) <= objects[i].radius + objects[j].radius)
        return false;
  return true;
}

void Geometry::initBground(ElectroMagnetic bground_) { bground = bground_; }

optimet::t_uint Geometry::scatterer_size() const {
  auto const object_size = [](optimet::t_uint current, Scatterer const &scatterer) {
    return current + 2 * scatterer.nMax * (scatterer.nMax + 2);
  };
  return std::accumulate(objects.cbegin(), objects.cend(), 0, object_size);
}

bool Geometry::no_overlap(Scatterer const &object_) {
  if(objects.size() < 1)
    return true;

  for(auto const &obj : objects)
    if(Tools::findDistance(obj.vR, object_.vR) <= (object_.radius + obj.radius))
      return false;

  return true;
}

// AJ
// -----------------------------------------------------------------------------------------------------
int Geometry::getNLSources(double omega_, int objectIndex_, int nMax_,
                           std::complex<double> *sourceU, std::complex<double> *sourceV) const {
  double R = objects[objectIndex_].radius;

  std::complex<double> mu_b = bground.mu;
  std::complex<double> eps_b = bground.epsilon;
  std::complex<double> mu_j2 = objects[objectIndex_].elmag.mu;       /* AJ - SH sphere eps - AJ */
  std::complex<double> eps_j2 = objects[objectIndex_].elmag.epsilon; /* AJ - SH sphere eps - AJ */

  // T_2w_ Auxiliary variables
  // --------------------------------------------------------------------------------
  std::complex<double> k_j2 = omega_ * std::sqrt(eps_j2 * mu_j2);
  std::complex<double> k_b2 = omega_ * std::sqrt(eps_b * mu_b);
  std::complex<double> x_j2 = k_j2 * R;
  std::complex<double> x_b2 = k_b2 * R;
  std::complex<double> zeta_b2 = std::sqrt(mu_b / eps_b);
  std::complex<double> zeta_j2 = std::sqrt(mu_j2 / eps_j2);
  std::complex<double> zeta_boj2 = zeta_b2 / zeta_j2;

  // Auxiliary Riccati-Bessel functions
  // ---------------------------------------------------------------------
  std::complex<double> psi2(0., 0.), xsi2(0., 0.);
  std::complex<double> dpsi2(0., 0.), dxsi2(0., 0.);

  std::vector<std::complex<double>> J_n_data, J_n_ddata;
  std::vector<std::complex<double>> H_n_data, H_n_ddata;

  try {
    std::tie(J_n_data, J_n_ddata) = optimet::bessel<optimet::Bessel>(x_j2, nMax_);
    std::tie(H_n_data, H_n_ddata) = optimet::bessel<optimet::Hankel1>(x_b2, nMax_);
  } catch(std::range_error &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  CompoundIterator p;

  int pMax = p.max(nMax_);

  for(p = 0; (int)p < pMax; p++) {
    // obtain aux Riccati-Bessel functions ---------------------------
    // psi
    psi2 = x_j2 * J_n_data[p.first];
    dpsi2 = x_j2 * J_n_ddata[p.first] + J_n_data[p.first];
    // ksi
    xsi2 = x_b2 * H_n_data[p.first];
    dxsi2 = x_b2 * H_n_ddata[p.first] + H_n_data[p.first];

    // obtain diagonal source matrices --------------------------------
    // SRC_2w_p - TE Part
    sourceU[p] = x_b2 * dpsi2 / (xsi2 * dpsi2 - zeta_boj2 * psi2 * dxsi2);                    // u'
    sourceU[p + nMax_] = zeta_boj2 * x_b2 * psi2 / (zeta_boj2 * psi2 * dxsi2 - xsi2 * dpsi2); // u''

    // SRC_2w_p - TM part
    sourceV[p] = x_b2 * psi2 / (zeta_boj2 * xsi2 * dpsi2 - psi2 * dxsi2); // v'
    sourceV[p + nMax_] = std::complex<double>(0., 0.);                    // v''
  }

  return 0;
}

int Geometry::getCabsAux(double omega_, int objectIndex_, int nMax_, double *Cabs_aux_) {

  std::complex<double> temp1(0., 0.);
  double temp2(0.);
  std::complex<double> k_s =
      omega_ * std::sqrt(objects[objectIndex_].elmag.epsilon * objects[objectIndex_].elmag.mu);
  std::complex<double> k_b = omega_ * std::sqrt(bground.epsilon * bground.mu);

  std::complex<double> rho = k_s / k_b;

  std::complex<double> r_0 = k_b * objects[objectIndex_].radius;

  std::complex<double> mu_j = objects[objectIndex_].elmag.mu;
  std::complex<double> mu_0 = bground.mu;

  std::complex<double> psi(0., 0.), ksi(0., 0.);
  std::complex<double> dpsi(0., 0.), dksi(0., 0.);
  std::complex<double> psirho(0., 0.);
  std::complex<double> dpsirho(0., 0.);

  std::vector<std::complex<double>> J_n_data, J_n_ddata;
  std::vector<std::complex<double>> Jrho_n_data, Jrho_n_ddata;

  try {
    std::tie(J_n_data, J_n_ddata) = optimet::bessel<optimet::Bessel>(r_0, nMax_);
    std::tie(Jrho_n_data, Jrho_n_ddata) = optimet::bessel<optimet::Bessel>(rho * r_0, nMax_);
  } catch(std::range_error &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  CompoundIterator p;

  int pMax = p.max(nMax_);

  for(p = 0; (int)p < pMax; p++) {
    // obtain Riccati-Bessel functions
    psi = r_0 * J_n_data[p.first];
    dpsi = r_0 * J_n_ddata[p.first] + J_n_data[p.first];

    psirho = r_0 * rho * Jrho_n_data[p.first];
    dpsirho = r_0 * rho * Jrho_n_ddata[p.first] + Jrho_n_data[p.first];

    // Stout 2002 - from scattered
    // TE Part
    temp1 = std::complex<double>(0., 1.) * rho * mu_0 * conj(mu_j) * conj(psirho) * dpsirho;
    temp2 = abs((mu_j * psirho * dpsi - mu_0 * rho * dpsirho * psi));
    temp2 *= temp2;
    Cabs_aux_[p] = real(temp1) / temp2;

    // TM part
    temp1 = std::complex<double>(0., 1.) * conj(rho) * mu_0 * mu_j * conj(psirho) * dpsirho;
    temp2 = abs((mu_0 * rho * psirho * dpsi - mu_j * dpsirho * psi));
    temp2 *= temp2;
    Cabs_aux_[(int)p + pMax] = real(temp1) / temp2;
  }
  return 0;
}

Spherical<double> Geometry::translateCoordinates(int firstObject_, int secondObject_) {
  return objects[firstObject_].vR - objects[secondObject_].vR;
}

int Geometry::checkInner(Spherical<double> R_) {
  Spherical<double> Rrel;

  for(size_t j = 0; j < objects.size(); j++) {
    // Translate to object j
    Rrel = Tools::toPoint(R_, objects[j].vR);

    // Check if R_.rrr is inside radius of object j
    if(Rrel.rrr <= objects[j].radius)
      return j;
  }

  return -1;
}

int Geometry::setSourcesSingle(std::shared_ptr<optimet::Excitation const> incWave_,
                               std::complex<double> const *internalCoef_FF_, int nMax_) {
  CompoundIterator p;
  int pMax = p.max(nMax_);

  std::complex<double> *sourceU = new std::complex<double>[2 * pMax];
  std::complex<double> *sourceV = new std::complex<double>[2 * pMax];

  auto const omega = incWave_->omega();
  for(size_t j = 0; j < objects.size(); j++) {
    getNLSources(omega, j, nMax_, sourceU, sourceV);

    for(p = 0; p < pMax; p++) {
      objects[j].sourceCoef[static_cast<int>(p)] =
          sourceU[p] * optimet::symbol::up_mn(p.second, p.first, nMax_,
                                              internalCoef_FF_[j * 2 * pMax + p.compound],
                                              internalCoef_FF_[pMax + j * 2 * pMax + p.compound],
                                              omega, objects[j], bground) +
          sourceV[p] * optimet::symbol::vp_mn(p.second, p.first, nMax_,
                                              internalCoef_FF_[j * 2 * pMax + p.compound],
                                              internalCoef_FF_[pMax + j * 2 * pMax + p.compound],
                                              omega, objects[j], bground);

      objects[j].sourceCoef[static_cast<int>(p) + pMax] =
          sourceU[p + pMax] *
              optimet::symbol::upp_mn(
                  p.second, p.first, nMax_, internalCoef_FF_[j * 2 * pMax + p.compound],
                  internalCoef_FF_[pMax + j * 2 * pMax + p.compound], omega, objects[j]) +
          sourceV[p + pMax]; //<- this last bit is zero for the moment
    }
  }

  delete[] sourceU;
  delete[] sourceV;

  return 0;
}

optimet::Vector<optimet::t_complex> Geometry::getTLocal(optimet::t_real omega_,
                                                        optimet::t_int objectIndex_,
                                                        optimet::t_uint nMax_) const {
  if(objectIndex_ >= static_cast<int>(objects.size()))
    std::out_of_range("Object index out of range");
  if(nMax_ != static_cast<optimet::t_uint>(objects[objectIndex_].nMax))
    std::runtime_error("Internal inconsistency");
  return objects[objectIndex_].getTLocal(omega_, bground);
}

int Geometry::getSourceLocal(int objectIndex_, std::shared_ptr<optimet::Excitation const> incWave_,
                             int nMax_, std::complex<double> *Q_SH_local_) const {
  CompoundIterator p;
  CompoundIterator q;

  int pMax = p.max(nMax_);
  int qMax = q.max(nMax_);

  // Make sure Q_SH_local_ is zero
  for(p = 0; p < pMax; p++) {
    Q_SH_local_[p] = std::complex<double>(0.0, 0.0);
    Q_SH_local_[p + pMax] = std::complex<double>(0.0, 0.0);
  }

  std::complex<double> **T_AB = Tools::Get_2D_c_double(2 * pMax, 2 * qMax);
  std::complex<double> *Q_interm = new std::complex<double>[pMax + qMax];

  for(size_t j = 0; j < objects.size(); j++) {
    if(static_cast<int>(j) == objectIndex_)
      continue;

    // Build the T_AB matrix
    optimet::Coupling const AB(objects[objectIndex_].vR - objects[j].vR, incWave_->waveK, nMax_);

    for(p = 0; p < pMax; p++) {
      for(q = 0; q < qMax; q++) {
        T_AB[p][q] = AB.diagonal(p, q);
        T_AB[p + pMax][q + qMax] = AB.diagonal(p, q);
        T_AB[p + pMax][q] = AB.offdiagonal(p, q);
        T_AB[p][q + qMax] = AB.offdiagonal(p, q);
      }
    }

    // Multiply T_AB with the single local sources
    Algebra::multiplyVectorMatrix(T_AB, 2 * pMax, 2 * qMax, objects[j].sourceCoef.data(), Q_interm,
                                  consC1, consC1);

    // Finally, add Q_interm to Q_SH_local
    for(p = 0; p < pMax; p++) {
      Q_SH_local_[p] += Q_interm[p];
      Q_SH_local_[p + pMax] += Q_interm[p + pMax];
    }
  }

  // Clear up the memory
  for(int i = 0; i < 2 * pMax; i++)
    delete[] T_AB[i];

  delete[] T_AB;
  delete[] Q_interm;

  return 0;
}

void Geometry::update(std::shared_ptr<optimet::Excitation const> incWave_) {
  // Update the ElectroMagnetic properties of each object
  for(auto &object : objects)
    object.elmag.update(incWave_->lambda());
}

void Geometry::updateRadius(double radius_, int object_) { objects[object_].radius = radius_; }

void Geometry::rebuildStructure() {
  if(structureType == 1) {
    // Spiral structure needs to be rebuilt
    double R, d;
    int Np, No;
    double Theta;

    Np = ((objects.size() - 1) / 4) + 1;

    Theta = consPi / (Np - 1); // Calculate the separation angle

    d = 2 * (spiralSeparation + 2 * objects[0].radius);
    R = d / (4 * sin(Theta / 2.0));

    No = objects.size();

    // Create vectors for r, theta, x and y, X and Y
    std::vector<double> X(No - 1);
    std::vector<double> Y(No - 1);

    int j = 0;

    //"Vertical" arms
    for(int i = 0; i < Np; i++) {
      double x_;
      double y_;

      Tools::Pol2Cart(R, i * Theta, x_, y_);

      if(i == 0) {
        X[j] = x_ + R;
        Y[j] = y_;
        j++;
        continue;
      }

      if(i == Np - 1) {
        X[j] = x_ - R;
        Y[j] = -1.0 * y_;
        j++;
        continue;
      }

      X[j] = x_ + R;
      Y[j] = y_;
      j++;

      X[j] = x_ - R;
      Y[j] = -1.0 * y_;
      j++;
    }

    //"Horizontal" arms
    for(int i = 0; i < Np; i++) {
      double x_;
      double y_;
      Tools::Pol2Cart(R, i * Theta - consPi / 2, x_, y_);

      if(i == 0) {
        X[j] = x_;
        Y[j] = y_ - R;
        j++;
        continue;
      }

      if(i == Np - 1) {
        X[j] = -1.0 * x_;
        Y[j] = y_ + R;
        j++;
        continue;
      }

      X[j] = -1.0 * x_;
      Y[j] = y_ + R;
      j++;

      X[j] = x_;
      Y[j] = y_ - R;
      j++;
    }

    // Determine normal, convert to a spherical object and push

    Cartesian<double> auxCar(0.0, 0.0, 0.0);
    Spherical<double> auxSph(0.0, 0.0, 0.0);

    for(int i = 0; i < No - 1; i++) {
      if(normalToSpiral == 0) {
        // x is normal (conversion is x(pol) -> y; y(pol) -> z
        auxCar.x = 0.0;
        auxCar.y = X[i];
        auxCar.z = Y[i];
      }

      if(normalToSpiral == 1) {
        // y is normal (conversion is x(pol) -> z; y(pol) -> x
        auxCar.x = Y[i];
        auxCar.y = 0.0;
        auxCar.z = X[i];
      }

      if(normalToSpiral == 2) {
        // z is normal (conversion is x(pol) -> x; y(pol) -> x
        auxCar.x = X[i];
        auxCar.y = Y[i];
        auxCar.z = 0.0;
      }

      auxSph = Tools::toSpherical(auxCar);
      objects[i + 1].vR = auxSph;
    }
  }
}
