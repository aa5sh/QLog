# DXCC flag policy

`res/data/dxcc.json` maps ADIF DXCC entity IDs to optional flag icons. The
icons are visual identifiers only. They do not express a QLog position on the
sovereignty or political status of an entity.

Apply the following rules in order:

1. DXCC entity names and numeric codes follow ADIF.
2. Use an entity's own established and commonly recognized flag or regional
   symbol when it is also used by a recognized standard, system, government,
   or application.
3. If the entity has no such symbol, a national flag may be used only when it
   does not require QLog to select one side of a documented territorial
   dispute.
4. If competing sovereignty claims are documented and the entity has no
   distinct symbol, set `flag` to `null`.
5. An authoritative international judgment or treaty can establish the flag
   assignment even when an older assignment or claim differs.

Two-letter values normally identify the corresponding standardized regional
or national flag included in `res/flags`. Named values such as `scotland`
identify an established regional symbol that has no suitable two-letter asset.

Every non-obvious decision added or reviewed under this policy must have an
`_comment` value in this form:

`Decision: <flag or no flag>. Basis: <rule applied>. Source(s): <URL(s)>`

This is required when a flag is removed because of a dispute, selected because
of a judgment or treaty, retained as a regional identifier in a politically
sensitive case, or corrected from a non-obvious legacy mapping. Prefer primary
sources such as international courts, treaty collections, standards bodies,
and governments. An amateur-radio organization is also suitable when the
evidence concerns the DXCC classification rather than territorial sovereignty.

The policy originates from the analysis recorded in
https://github.com/foldynl/QLog/issues/1129#issuecomment-5250893292.
