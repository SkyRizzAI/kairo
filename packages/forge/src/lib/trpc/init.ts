import { initTRPC } from '@trpc/server';
import type { Context } from './context';

// One tRPC instance for the whole Forge backend.
const t = initTRPC.context<Context>().create();

export const router = t.router;
export const publicProcedure = t.procedure;
