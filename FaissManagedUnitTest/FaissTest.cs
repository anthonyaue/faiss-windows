﻿using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Faiss;
using System.Text;

namespace FaissManagedUnitTest
{

    
    [TestClass]
    public class FaissTest 
    {

        private void DumpArray<T>(T[] dumpMe, int rows, int cols, string name="")
        {
            if(!string.IsNullOrWhiteSpace(name))
            {
                TestContext.WriteLine(name);
            }
            for(int i=0; i < rows; ++i)
            {
                StringBuilder sb = new StringBuilder();
                for (int j = 0; j < cols; ++j)
                {
                    sb.Append(dumpMe[i * cols + j]);
                    sb.Append(" ");
                }
                TestContext.WriteLine(sb.ToString());
            }
        }
        public TestContext TestContext { get; set; }
        [TestMethod]
        public void IndexFlatL2Test()
        {
            // Dimension of vectors (# cols)
            int d = 3;
            // Num vectors
            int v = 5;
            // k-best to search
            int k = 3;
            var idx = new IndexFlatL2(d);
            float[] addMe = new float[d * v];
            for(int i=0; i < v; ++i)
            {
                for (int j = 0; j < d; ++j)
                {
                    addMe[i * d + j] = (float)j + (.1f * (float)i);
                }
            }
            DumpArray(addMe, v, d, "addMe");
            idx.Add(v, addMe);
            float[] dist = new float[v * k];
            Int64[] labels = new long[v * k];
            idx.Search(v, addMe, k, dist, labels);
            DumpArray(labels, v, k, "labels");
            DumpArray(dist, v, k, "distances");

            for(int i=0; i < v; ++i)
            {
                // Distance from self should be zero
                Assert.IsTrue(dist[i * k] == 0.0f);
                // Distance to anyone else should be > 0
                Assert.IsTrue(dist[(i * k) + 1] > 0.0f);
                // We should always be our own one-best
                Assert.IsTrue(labels[i * k] == i);
            }
            
            Assert.IsTrue(1 == 1);
        }
    }
}
