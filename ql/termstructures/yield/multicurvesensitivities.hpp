/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
 Copyright (C) 2016 Michael von den Driesch

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file multicurvesensitivity.hpp
\brief compute sensitivities based of traits (zeroyield, discount, forward) to the input 
input instruments (par quotes).
*/

#ifndef quantlib_multicurve_sensitivity_hpp
#define quantlib_multicurve_sensitivity_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <boost/shared_ptr.hpp>

namespace {
QuantLib::Real secondElement(const std::pair< QuantLib::Date, QuantLib::Real > &p) { return p.second; }
}

namespace QuantLib {

class MultiCurveSensitivities : public LazyObject {
private:
  typedef std::map< std::string, Handle< YieldTermStructure > > curvespec;

public:
  MultiCurveSensitivities(curvespec curves) : curves_(curves) {
    for (curvespec::const_iterator it = curves_.begin(); it!=curves_.end(); ++it)
      registerWith((*it).second);
    for (curvespec::const_iterator it = curves_.begin(); it != curves_.end(); ++it) {
      boost::shared_ptr< PiecewiseYieldCurve< ZeroYield, Linear > > curve =
          boost::dynamic_pointer_cast< PiecewiseYieldCurve< ZeroYield, Linear > >(it->second.currentLink());
      QL_REQUIRE(curve != NULL, "Couldn't cast curvename: " << it->first);
      for (std::vector< boost::shared_ptr< BootstrapHelper< YieldTermStructure > > >::iterator it =
               curve->instruments_.begin();
           it != curve->instruments_.end(); ++it) {
        allQuotes_.push_back((*it)->quote());
      }
    }
  }

  Matrix getSensi() const;
  Matrix getInverseSensi() const;
  //! \name Observer interface
  //@{
  void update();
  //@}

private:
  //! \name LazyObject interface
  //@{
  void performCalculations() const;
  //@}
  // methods
  std::vector< Real > allZeros() const;
  std::vector< std::pair< Date, Real > > allNodes() const;
  mutable std::vector< Rate > origZeros_;
  std::vector< Handle< Quote > > allQuotes_;
  std::vector< std::pair< Date, Real > > origNodes_;
  mutable Matrix sensi_, invSensi_;
  curvespec curves_;
};

void MultiCurveSensitivities::performCalculations() const {
  std::vector< Rate > sensiVector;
  origZeros_ = allZeros();
  for (std::vector< Handle< Quote > >::const_iterator it = allQuotes_.begin(); it != allQuotes_.end(); ++it) {
    Rate bps = 1e-4;
    Rate origQuote = (*it)->value();
    boost::shared_ptr< SimpleQuote > q = boost::dynamic_pointer_cast< SimpleQuote >((*it).currentLink());
    q->setValue(origQuote + bps);
    std::vector< Rate > tmp(allZeros());
    for (Size i = 0; i < tmp.size(); ++i)
        sensiVector.push_back((tmp[i] - origZeros_[i])/bps);
    q->setValue(origQuote);
  }
  Matrix result(origZeros_.size(), origZeros_.size(), sensiVector);
  sensi_ = result;
  invSensi_ = inverse(sensi_);
}

void MultiCurveSensitivities::update() { LazyObject::update(); }

Matrix MultiCurveSensitivities::getSensi() const {
  calculate();
  return sensi_;
}

Matrix MultiCurveSensitivities::getInverseSensi() const {
  calculate();
  return invSensi_;
}

std::vector< std::pair< Date, Real > > MultiCurveSensitivities::allNodes() const {
  std::vector< std::pair< Date, Real > > result;
  for (curvespec::const_iterator it = curves_.begin(); it != curves_.end(); ++it) {
    boost::shared_ptr< PiecewiseYieldCurve< ZeroYield, Linear > > curve =
        boost::dynamic_pointer_cast< PiecewiseYieldCurve< ZeroYield, Linear > >(it->second.currentLink());
    result.reserve(result.size() + curve->nodes().size() - 1);
    for (auto p = curve->nodes().begin() + 1; p != curve->nodes().end(); ++p)
      result.push_back(*p);
  }
  return result;
}

std::vector< Real > MultiCurveSensitivities::allZeros() const {
  std::vector< std::pair< Date, Real > > result = allNodes();
  std::vector< Real > zeros;
  std::transform(result.begin(), result.end(), std::back_inserter(zeros), secondElement);
  return zeros;
}
}

#endif
