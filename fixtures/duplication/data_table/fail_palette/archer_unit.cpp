// Fail fixture: a color-tier table copied between two unit files, differing only
// in the `default:` RGB. High line overlap (the cases are byte-identical) so the
// pair clears the joint floor and reaches the data-table phase; only literals
// differ so cloneType is LITERAL and diversity is low. Modelled on the real
// djeada/Standard-of-Iron team_color FP. The guard MUST drop this pair.
// The second function gives the file a second fragment so the IDF is realistic
// (an isolated two-fragment file scores the table pair below the gate).

static inline auto team_color(int owner_id) -> Vec3
{
  switch (owner_id)
  {
  case 1:
    return {0.20F, 0.55F, 1.00F};
  case 2:
    return {1.00F, 0.30F, 0.30F};
  case 3:
    return {0.20F, 0.80F, 0.40F};
  case 4:
    return {1.00F, 0.80F, 0.20F};
  default:
    return {0.80F, 0.90F, 1.00F};
  }
}

void archerInit(World &world, const SpawnParams &params)
{
  auto unit = makeArcher(world);
  unit->setRange(12);
  unit->setSpeed(3);
  unit->attachQuiver(params.ammo);
  registerUnit(world, std::move(unit));
}
