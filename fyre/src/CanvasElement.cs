/*
 * CanvasElement - flyweight which handles drawing of an Element
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

using System.Drawing;
using System.Xml;

namespace Fyre
{
	/*
	 * The basic structure of an element on the canvas has 3 pieces:
	 * 	name
	 * 	pads
	 * 	element-specific data
	 *
	 * Name and pads are provided by this class. If an element requires
	 * custom drawings (such as equations, etc), it will subclass
	 * CanvasElement.
	 *
	 * y-dimensions:
	 * 	name:			20px
	 * 	pad:			20px
	 * 	inter-pad distance:	10px
	 * 	top-to-pad:		8px
	 * 	pad-to-bottom:		6px
	 *
	 * x-dimensions:
	 * 	minimum size is either name, extra-data or pads
	 * 	pad-to-name:		7px
	 * 	between-names:		25px
	 */

	public class CanvasElement
	{
		// Width and height are maintained by the internal layout system.
		// X & Y are maintained by the global layout system. We'll probably
		// want get/set operators on this to trigger redraws, etc.
		public Rectangle		position;
		private Pen			pen;
		static private Gdk.Pixmap	pm = new Gdk.Pixmap (null,1,1,16);

		public
		CanvasElement (Element e)
		{
			Graphics	graphics = Gtk.DotNet.Graphics.FromDrawable (pm);
			Font		font = new Font (new FontFamily ("Bitstream Vera Sans"), 10, FontStyle.Regular);
			int 		numpads;

			if (e.inputs.Length > e.outputs.Length)
				numpads = e.inputs.Length;
			else
				numpads = e.outputs.Length;

			position.Height = 14 + (numpads - 1)*font.Height + numpads*20;

			position.Width  = 14;
			position.Width += (int) System.Math.Ceiling (graphics.MeasureString (e.LongestInputPadName (), font).Width);
			position.Width += (int) System.Math.Ceiling (graphics.MeasureString (e.LongestOutputPadName (), font).Width);

			pen = new Pen (Color.Azure);
		}

		// When we're drawing on the main canvas, we render at full size.
		// However, we also want to be able to render smaller for the
		// navigation box, so there are two versions.
		public virtual void
		Draw (System.Drawing.Graphics context, float zoom)
		{
			// FIXME
		}

		public virtual void
		Draw (System.Drawing.Graphics context)
		{
			// FIXME
			context.DrawRectangle (pen, position);
		}

		public virtual void
		Serialize (XmlTextWriter writer)
		{
			// TODO: Implement
		}
	}

}
