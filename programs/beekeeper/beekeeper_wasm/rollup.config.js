import dts from 'rollup-plugin-dts';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import replace from '@rollup/plugin-replace';
import copy from 'rollup-plugin-copy';

export default [
  {
    input: `dist/detailed/index.js`,
    output: {
      format: 'es',
      file: `dist/bundle/detailed/index.js`,
      paths: (id) => {
        // Rewrite path so it points to ../build_minimal/ from dist/bundle/detailed/
        if (id.includes('beekeeper_minimal')) {
          return '../build_minimal/beekeeper_minimal.js';
        }
        return id;
      }
    },
    external: (id) => id.includes('beekeeper_minimal'),
    plugins: [
      copy({
        targets: [
          { src: ['src/build_minimal/beekeeper_minimal.js', 'src/build_minimal/beekeeper_minimal.wasm'], dest: 'dist/bundle/build_minimal' }
        ],
        hook: 'buildStart'
      }),
      replace({
        values: {
          'process.env.npm_package_version': `"${process.env.npm_package_version}"`
        },
        preventAssignment: true
      }),
      nodeResolve({ preferBuiltins: false, browser: false }),
      commonjs()
    ]
  },
  {
    input: 'dist/index.js',
    output: {
      format: 'es',
      file: 'dist/bundle/web.js'
    },
    external: (id) => id.includes('beekeeper_minimal') || id.includes('./detailed/index'),
    plugins: [
      replace({
        values: {
          'process.env.DEFAULT_STORAGE_ROOT': `"/storage_root"`,
          'process.env.ROLLUP_TARGET_ENV': `"web"`
        },
        preventAssignment: true
      })
    ]
  },
  {
    input: 'dist/index.js',
    output: {
      format: 'es',
      file: 'dist/bundle/node.js'
    },
    external: (id) => id.includes('beekeeper_minimal') || id.includes('./detailed/index'),
    plugins: [
      replace({
        values: {
          'process.env.DEFAULT_STORAGE_ROOT': `"./storage_root-node"`,
          'process.env.ROLLUP_TARGET_ENV': `"node"`
        },
        preventAssignment: true
      })
    ]
  },
  {
    input: `dist/index.d.ts`,
    output: [
      { file: `dist/bundle/index.d.ts`, format: "es" }
    ],
    plugins: [
      dts()
    ]
  }
];
