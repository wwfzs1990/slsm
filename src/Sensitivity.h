/*
  Copyright (c) 2015-2017 Lester Hedges <lester.hedges+slsm@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SENSITIVITY_H
#define _SENSITIVITY_H

#include <functional>

/*! \file Sensitivity.h
    \brief A class for calculating finite-difference boundary point sensitivities.
 */

namespace slsm
{
    // FORWARD DECLARATIONS

    struct BoundaryPoint;

    //! Calculate the value of a function for a small displacement of a boundary point.
    /*! \param point
            A reference to the boundary point.

        \return
            The value of the function.
     */
    typedef std::function<double (const BoundaryPoint&)> SensitivityCallback;

#ifdef PYBIND
    //! Wrapper structure to expose the callback function to Python.
    class Callback
    {
    public:
        //! Constructor.
        Callback();

        /// The callback function.
        SensitivityCallback callback;
    };
#endif

    /*! \brief A class for calculating finite-difference boundary point sensitivities.
    */
    class Sensitivity
    {
    public:
        //! Constructor.
        /*! \param delta_
                The finite-difference derivative length (perturbation).
        */
        Sensitivity(double delta_ = 1e-4);

        //! Compute finite-difference sensitivity for an arbitrary function.
        /*! \param point
                A reference to the boundary point.

            \param callback
                A reference to the sensitivity callback function.

            \return
                The finite-difference sensitivity.
         */
        double computeSensitivity(BoundaryPoint&, SensitivityCallback&) const;

        //! Apply deterministic Ito correction to objective sensitivity.
        /*! \param boundary
                A reference to the discretised boundary.

            \param levelSet
                A reference to the level set object.

            \param temperature
                The temperature of the thermal bath.

         */
        void itoCorrection(Boundary&, const LevelSet&, double) const;

        //! Apply deterministic Ito correction to objective sensitivity.
        /*! \param boundary
                A reference to the discretised boundary.

            \param temperature
                The temperature of the thermal bath.

         */
        void itoCorrection(Boundary&, double) const;

    private:
        /// The finite-difference derivative length (in units of the grid spacing).
        double delta;
    };
}

#endif	/* _SENSITIVITY_H */
