/*
 * CartesianProduct.cs - An Element which takes two ranges of integers
 *	and outputs a matrix of the cartesian product.
 *	FIXME - we may want to have this take other "generators" as inputs,
 *	rather than just restricting it to integers
 *
 * Fyre - a generic framework for computational art
 * Copyright (C) 2004-2005 Fyre Team (see AUTHORS)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

using System.Xml;

class CartesianProduct : Fyre.Element
{
	static Gdk.Pixbuf icon;

	public
	CartesianProduct ()
	{
		inputs = new Fyre.InputPad[] {
			new Fyre.InputPad ("(x<sub>0</sub>,x<sub>1</sub>)", "X range"),
			new Fyre.InputPad ("(y<sub>0</sub>,y<sub>1</sub>)", "Y range"),
		};

		outputs = new Fyre.OutputPad[] {
			new Fyre.OutputPad ("M", "matrix"),
		};

		SetPadNumbers ();
		NewCanvasElement ();
		NewID ();
	}

	public override string
	Name ()
	{
		return "Cartesian Product";
	}

	public override string
	Category()
	{
		return "Arithmetic";
	}

	public override Gdk.Pixbuf
	Icon ()
	{
		if (icon == null)
			icon = new Gdk.Pixbuf (null, "CartesianProduct.png");
		return icon;
	}

	public override string
	Description ()
	{
		return "Computes the cartesian product\nof two integer ranges";
	}

	public override bool
	Check (Fyre.Type[] t, out Fyre.Type[] to)
	{
		// FIXME - set type
		to = null;
		if (t[0] is Fyre.Matrix && t[1] is Fyre.Matrix) {
			Fyre.Matrix m = (Fyre.Matrix) t[0];
			// Make sure we've got 2-vectors for both parameters
			if (!(m.Rank == 1 && m.Size[0] == 2))
				return false;
			if (m.Rank == 1 && m.Size[0] == 2)
				return true;
		}
		return false;
	}
}
