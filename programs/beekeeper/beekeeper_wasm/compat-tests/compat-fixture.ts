/**
 * Playwright fixture that runs each test function in both Node.js and Chromium,
 * then asserts that results are identical across environments.
 *
 * Follows the same fn.toString() + page.evaluate() pattern as the main
 * beekeeper_wasm test suite (see __tests__/assets/jest-helper.ts).
 */
import { test as base, expect, type Page } from '@playwright/test';

type CompatGlobals = {
  [Symbol.asyncDispose]: () => Promise<void>;
  createOld: (options?: Record<string, unknown>) => Promise<any>;
  createNew: (options?: Record<string, unknown>) => Promise<any>;
};

type CompatTestFn<R, Args extends any[]> = (globals: CompatGlobals, ...args: Args) => Promise<R>;
type CompatRunner = <R, Args extends any[]>(fn: CompatTestFn<R, Args>, ...args: Args) => Promise<R>;

export interface ICompatFixtures {
  /** Runs fn in both Node.js and Chromium, asserts identical results */
  compatTest: CompatRunner;
}

const alreadyConsoleLogInitialized = new WeakSet<Page>();

function createNodeGlobals(): CompatGlobals {
  const nodes: Array<{ delete: () => (Promise<void> | void) }> = [];

  return {
    [Symbol.asyncDispose]: async () => {
      await Promise.allSettled(nodes.map(node => node.delete()));
    },
    createOld: async (options) => {
      const mod = await import('@hiveio/beekeeper-old');
      const instance = await (mod.default as any)(options);
      nodes.push(instance);
      return instance;
    },
    createNew: async (options) => {
      const mod = await import('@hiveio/beekeeper');
      const instance = await mod.default(options);
      nodes.push(instance);
      return instance;
    },
  };
}

export const test = base.extend<ICompatFixtures>({
  compatTest: async ({ page }, use) => {
    const runner: CompatRunner = async (fn, ...args) => {
      // --- Node.js ---
      await using nodeGlobals = createNodeGlobals();
      const nodeResult = await fn(nodeGlobals, ...args);

      // --- Chromium ---
      if (!alreadyConsoleLogInitialized.has(page)) {
        page.on('console', msg => console.log('>>', msg.type(), msg.text()));
        alreadyConsoleLogInitialized.add(page);
      }

      await page.goto('http://localhost:8090/', { waitUntil: 'load' });
      await page.waitForFunction(() => (window as any).__ready === true, undefined, { timeout: 30_000 });

      const webResult = await page.evaluate(
        async ({ fnStr, args }) => {
          const globals = {
            createOld: (window as any).createOldBeekeeper,
            createNew: (window as any).createNewBeekeeper,
          };
          const fn = new Function('return ' + fnStr)();
          return await fn(globals, ...args);
        },
        { fnStr: fn.toString(), args },
      );

      // Normalize nodeResult (strip prototype data for comparison)
      const normalizedNode = JSON.parse(JSON.stringify(nodeResult));
      expect(webResult).toStrictEqual(normalizedNode);

      return webResult;
    };

    use(runner);
  },
});

export { expect };
