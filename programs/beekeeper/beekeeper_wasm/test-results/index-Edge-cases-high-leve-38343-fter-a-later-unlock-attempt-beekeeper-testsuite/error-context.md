# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: __tests__/index.spec.ts >> Edge cases: high-level factory API >> Should keep timed-out wallets locked after a later unlock attempt
- Location: __tests__/detailed/edge_cases.ts:665:3

# Error details

```
BeekeeperError: Wallet locked due to timeout
```

# Test source

```ts
  109 | async aes256CbcDecrypt(key,iv,data){try{
  110 | const cryptoKey=await crypto.subtle.importKey("raw",key,"AES-CBC",!1,["decrypt"])
  111 | ;return new Uint8Array(await crypto.subtle.decrypt({name:"AES-CBC",iv},cryptoKey,data))}catch{
  112 | return null}},async pbkdf2HmacSha512(password,salt,iterations,dkLen){try{
  113 | const passwordKey=0===password.length?new Uint8Array(1):password,baseKey=await crypto.subtle.importKey("raw",passwordKey,"PBKDF2",!1,["deriveBits"]),bits=await crypto.subtle.deriveBits({
  114 | name:"PBKDF2",hash:"SHA-512",salt,iterations},baseKey,8*dkLen);return new Uint8Array(bits)}catch{
  115 | return null}},getRandomBytes(dest){crypto.getRandomValues(dest)}}}
  116 | const createBeekeeper=async(beekeeperContstructor,ModuleExt={},options,storage)=>{
  117 | const crypto=createCryptoCallbacks();storage??={save_fn:()=>{},load_fn:name=>{
  118 | throw new Error("No persistent storage: "+name)},list_dir_fn:()=>[]}
  119 | ;const beekeeperProvider=await safeAsyncWasmCall((()=>beekeeperContstructor(ModuleExt)),"Beekeeper WASM module loading")
  120 | ;return new BeekeeperApi(beekeeperProvider,options,storage,crypto)
  121 | },LEGACY_WALLET_EXT=".wallet",LEGACY_WALLET_DIR=".beekeeper",OBJECT_STORE_NAME="FILE_DATA",openDb=dbName=>new Promise(((resolve,reject)=>{
  122 | const req=indexedDB.open(dbName);req.onupgradeneeded=()=>req.result.createObjectStore("FILE_DATA"),
  123 | req.onsuccess=()=>{const db=req.result
  124 | ;if(db.objectStoreNames.contains("FILE_DATA"))resolve(db);else{db.close()
  125 | ;const upgradeReq=indexedDB.open(dbName,db.version+1)
  126 | ;upgradeReq.onupgradeneeded=()=>upgradeReq.result.createObjectStore("FILE_DATA"),
  127 | upgradeReq.onsuccess=()=>resolve(upgradeReq.result),upgradeReq.onerror=()=>reject(upgradeReq.error)}
  128 | },req.onerror=()=>reject(req.error)})),preloadAll=(db,prefix)=>new Promise(((resolve,reject)=>{
  129 | const cache=new Map,tx=db.transaction("FILE_DATA","readonly"),req=tx.objectStore("FILE_DATA").openCursor()
  130 | ;req.onsuccess=()=>{const cursor=req.result;if(cursor){const key=cursor.key
  131 | ;key.startsWith(prefix)&&key.endsWith(".wallet")&&cache.set(key,new Uint8Array(cursor.value.contents)),
  132 | cursor.continue()}},tx.oncomplete=()=>resolve(cache),tx.onerror=()=>reject(tx.error)
  133 | })),webStorageCreator=async dbName=>{
  134 | const db=await openDb(dbName),keyPrefix=`${dbName}/.beekeeper/`,cache=await preloadAll(db,keyPrefix),toKey=name=>`${keyPrefix}${name}.wallet`
  135 | ;let pendingWrites=[];return{save_fn:(name,data)=>{const copy=new Uint8Array(data)
  136 | ;cache.set(toKey(name),copy);const p=new Promise(((resolve,reject)=>{
  137 | const tx=db.transaction("FILE_DATA","readwrite");tx.objectStore("FILE_DATA").put({contents:copy
  138 | },toKey(name)),tx.oncomplete=()=>resolve(),tx.onerror=()=>reject(tx.error)}));pendingWrites.push(p)
  139 | },load_fn:name=>{const data=cache.get(toKey(name))
  140 | ;if(!data)throw new Error("Wallet not found: "+name);return data},list_dir_fn:()=>{const names=[]
  141 | ;for(const key of cache.keys())names.push(key.slice(keyPrefix.length,-7));return names},
  142 | sync:async()=>{const writes=pendingWrites;pendingWrites=[],await Promise.all(writes)},close:()=>{
  143 | db.close()}}}
  144 | ;export{BeekeeperError,createCryptoCallbacks,webStorageCreator as createWebStorage,createBeekeeper as default};
      |                                  ^ BeekeeperError: Wallet locked due to timeout
```