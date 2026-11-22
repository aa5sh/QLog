/* Journal epoch last seen for each sync peer. A change means the peer
 * restarted its journal and must be read again from the start. */
ALTER TABLE sync_peers ADD COLUMN epoch TEXT;
