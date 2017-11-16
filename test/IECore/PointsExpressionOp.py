##########################################################################
#
#  Copyright (c) 2007, Image Engine Design Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#     * Neither the name of Image Engine Design nor the names of any
#       other contributors to this software may be used to endorse or
#       promote products derived from this software without specific prior
#       written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import unittest
import IECore

class TestPointsExpressionTest( unittest.TestCase ) :

	def setUp( self ) :

		numPoints = 100

		points = IECore.V3fVectorData( numPoints )
		colors = IECore.Color3fVectorData( numPoints )
		ints = IECore.IntVectorData( numPoints )

		self.p = IECore.PointsPrimitive( numPoints )
		self.p["P"] = IECore.PrimitiveVariable( IECore.PrimitiveVariable.Interpolation.Vertex, points )
		self.p["Cs"] = IECore.PrimitiveVariable( IECore.PrimitiveVariable.Interpolation.Varying, colors )
		self.p["int"] = IECore.PrimitiveVariable( IECore.PrimitiveVariable.Interpolation.Varying, ints )

	def testRemoval( self ) :

		o = IECore.PointsExpressionOp()
		p = o( input = self.p, expression = "remove = i % 2" )

		self.assertEqual( p.numPoints, self.p.numPoints / 2 )
		for k in p.keys() :
			self.assertEqual( len( p[k].data ), len( self.p[k].data ) / 2 )

	def testAssignment( self ) :

		o = IECore.PointsExpressionOp()
		p = o( input = self.p, expression = "int = i * 10" )

		self.assertEqual( p.numPoints, self.p.numPoints )
		ints = p["int"].data
		for i in range( p.numPoints ) :
			self.assertEqual( i * 10, ints[i] )

	def testGlobals( self ) :

		o = IECore.PointsExpressionOp()
		p = o( input = self.p, expression = "P = V3f( i )" )

		points = p["P"].data
		for i in range( p.numPoints ) :
			self.assert_( points[i].equalWithAbsError( IECore.V3f( i ), 0.0001 ) )

if __name__ == "__main__":
	unittest.main()

