import { expect, test, type Page } from '@playwright/test';

const CSP = "script-src 'self' 'wasm-unsafe-eval'";
const CSP_PAGE_URL = 'http://localhost:8080/__tests__/assets/csp.html';

interface IBeekeeperCspWindow {
  beekeeperCspRun: Promise<{ publicKey: string; encrypted: string; decrypted: string; signature: string }>;
  cspViolations: string[];
}

const openCspPage = async (page: Page): Promise<void> => {
  await page.route(CSP_PAGE_URL, async route => {
    const response = await route.fetch();
    await route.fulfill({ response, headers: { ...response.headers(), 'content-security-policy': CSP } });
  });

  // Init scripts run outside the page's CSP, so the listener is in place before any page script
  await page.addInitScript(() => {
    const violations: string[] = [];
    (window as unknown as IBeekeeperCspWindow).cspViolations = violations;
    document.addEventListener('securitypolicyviolation', event => {
      violations.push(`${event.violatedDirective}: ${event.blockedURI}`);
    });
  });

  await page.goto(CSP_PAGE_URL, { waitUntil: 'load' });
};

const collectViolations = async (page: Page): Promise<string[]> => {
  // Violation events are dispatched asynchronously
  await page.waitForTimeout(100);

  return page.evaluate(() => (window as unknown as IBeekeeperCspWindow).cspViolations);
};

test.describe('Beekeeper under a strict Content-Security-Policy', () => {
  test('Should enforce the policy on the fixture page', async ({ page }) => {
    await openCspPage(page);

    // Code evaluated over DevTools is exempt from the eval check, so probe with a page-inserted inline script
    const inlineScriptRan = await page.evaluate(() => {
      const script = document.createElement('script');
      script.textContent = 'window.inlineScriptRan = true;';
      document.body.append(script);
      return (window as unknown as { inlineScriptRan?: boolean }).inlineScriptRan === true;
    });

    expect(inlineScriptRan).toBe(false);
    expect(await collectViolations(page)).toHaveLength(1);
  });

  test('Should create session, import key, encrypt, decrypt and sign without CSP violations', async ({ page }) => {
    await openCspPage(page);

    const result = await page.evaluate(() => (window as unknown as IBeekeeperCspWindow).beekeeperCspRun);

    expect(result.publicKey).toMatch(/^STM[1-9A-HJ-NP-Za-km-z]{50}$/);
    expect(result.encrypted).not.toBe('');
    expect(result.decrypted).toBe('csp memo');
    expect(result.signature).toBe('1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21');
    expect(await collectViolations(page)).toEqual([]);
  });
});
