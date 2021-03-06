##########################################################################
#
#  Copyright (c) 2007-2012, Image Engine Design Inc. All rights reserved.
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
import gc
import weakref
import threading

import IECore

class TestWrapperGarbageCollection( unittest.TestCase ) :

	def test( self ) :

		# collect garbage from previous tests
		gc.collect()
		IECore.RefCounted.collectGarbage()

		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )
		f = IECore.FileSequenceParameter( "f", "d" )
		# FileSequenceParameter is not a python type, so there is no wrapped instance
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

		# OptionalCompoundParameter is a python type
		c = IECore.OptionalCompoundParameter( "c", members = [ f ] )
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		del c
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

		c = IECore.OptionalCompoundParameter( "c", members = [ f ] )
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		c2 = IECore.OptionalCompoundParameter( "c", members = [ f ] )
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 2 )
		c3 = IECore.OptionalCompoundParameter( "c", members = [ f ] )
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 3 )
		del c
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 3 )
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 2 )
		del c3
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 2 )
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		del c2
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

		IECore.RefCounted.garbageCollectionThreshold = 10
		self.assertEqual( IECore.RefCounted.garbageCollectionThreshold, 10 )
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )
		c = []
		for i in range( 0, 9 ) :
			c.append( IECore.OptionalCompoundParameter( "c", members = [ f ] ) )
			self.assertEqual( IECore.RefCounted.numWrappedInstances(), i+1 )
		del c
		# the creation of this last wrapped object should trigger a garbage collection
		c = IECore.OptionalCompoundParameter( "c", members = [ f ] )
		# leaving us with only it left
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 1 )
		del c
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

	def test2( self ) :

		"""This test exposes a bug which caused memory to leak."""

		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

		class PythonOp( IECore.Op ) :

			def __init__( self ) :

				IECore.Op.__init__( self, "opDescription", IECore.StringParameter( name = "result", description = "", defaultValue = "" ) )
				self.parameters().addParameter( IECore.StringParameter( name = "name", description = "", defaultValue="john" ) )

		o = PythonOp()
		del o
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

	def testWeakRef( self ) :

		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )

		self.callbackCalled = False
		def callback( w ) :

			self.callbackCalled = True

		o = IECore.Op( "", IECore.IntParameter( "result", "" ) )
		w = weakref.ref( o, callback )
		self.assertTrue( w() is o )
		del o
		IECore.RefCounted.collectGarbage()
		self.assertEqual( IECore.RefCounted.numWrappedInstances(), 0 )
		self.assertEqual( self.callbackCalled, True )
		self.assertEqual( w(), None )

	def testThreading( self ) :

		class ClassWithDel :

			def __init__( self ) :

				pass

			def __del__( self ) :

				for i in range( 0, 100 ) :

					pass

		def f() :

			for i in range( 0, 100 ) :
				o = IECore.Op( "", IECore.IntParameter( "result", "" ) )
				# The ClassWithDel class defines a __del__ method.
				# This means that when it is deleted (when
				# WrapperGarbageCollector::collect() calls Py_DECREF( o ) )
				# arbitrary python code gets run. This in
				# turn means the interpreter might switch to
				# a different thread. Which also might call
				# WrapperGarbageCollector::collect(). Which
				# will cause chaos if WrapperGarbageCollector::collect()
				# isn't savvy to the possibility.
				#
				# In the wild it seems that this may be able to cause problems even
				# without explicitly calling gc.collect() and RefCounted.collectGarbage(),
				# but this test encourages things to go wrong by calling them regularly.
				o.s = ClassWithDel()
				if i % 10 == 0 :
					while gc.collect() :
						pass
					IECore.RefCounted.collectGarbage()

		for j in range( 0, 10 ) :
			threads = []
			for i in range( 0, 8 ) :
				t = threading.Thread( target = f )
				t.start()
				threads.append( t )

			for t in threads :
				t.join()

if __name__ == "__main__":
        unittest.main()
