// Fail fixture: the other side of the copied color-tier table (see archer_unit.cpp).
// Identical switch, only the `default:` RGB differs -> LITERAL, low diversity.

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
    return {0.80F, 0.80F, 0.80F};
  }
}

void swordsmanRaise(Board &board, const Mutation &mutation)
{
  auto handle = lookupSlot(board, mutation.key);
  applyDelta(handle, mutation.delta);
  board.commit(handle);
  notifyObservers(board, handle.id);
}
