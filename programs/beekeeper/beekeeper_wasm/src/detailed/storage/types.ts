/**
 * Abstract storage interface for wallet persistence.
 * Implementations handle platform-specific storage (IndexedDB, Node fs, etc.)
 */
export interface IStorage {
  /**
   * Initialize the storage backend
   */
  init(): Promise<void>;

  /**
   * Read file contents as bytes
   * @param path - relative path within storage root
   * @returns file contents or null if not found
   */
  read(path: string): Promise<Uint8Array | null>;

  /**
   * Write file contents
   * @param path - relative path within storage root
   * @param data - file contents
   */
  write(path: string, data: Uint8Array): Promise<void>;

  /**
   * Delete a file
   * @param path - relative path within storage root
   */
  delete(path: string): Promise<void>;

  /**
   * Check if file exists
   * @param path - relative path within storage root
   */
  exists(path: string): Promise<boolean>;

  /**
   * List files in a directory
   * @param path - relative path within storage root
   * @returns array of file names (not full paths)
   */
  list(path: string): Promise<string[]>;

  /**
   * Ensure directory exists
   * @param path - relative path within storage root
   */
  mkdir(path: string): Promise<void>;
}

/**
 * Storage factory function type
 */
export type StorageFactory = (storageRoot: string) => IStorage;
