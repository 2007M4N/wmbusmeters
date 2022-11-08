/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FORMULA_H
#define FORMULA_H

#include<memory>
#include<vector>

#include"units.h"

struct Meter;
struct FieldInfo;

struct Formula
{
    // Parse and return false if parse failed.
    virtual bool parse(Meter *m, const std::string &f) = 0;
    // Returns false if the parse has failed and the formula is invalid.
    virtual bool valid() = 0;
    // Returns lines of error messages.
    virtual std::string errors() = 0;
    // Calculate the formula. Returns nan if it could not be calculated.
    virtual double calculate(Unit to) = 0;
    // Clear the formula, ie drop any parsed tree.
    virtual void clear() = 0;
    // Return a regenerated formula string.
    virtual std::string str() = 0;
    // Return the formula in a format where the tree structure is explicit.
    virtual std::string tree() = 0;

    virtual ~Formula() = 0;
};

Formula *newFormula();

#endif
