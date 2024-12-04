import type { Component } from "solid-js";
import { splitProps } from "solid-js";

import { cva } from "class-variance-authority";

import { cn } from "~/lib/utils";
import type { BadgeProps } from "~/components/ui/badge";
import { Badge } from "~/components/ui/badge";

const badgeDeltaVariants = cva("", {
  variants: {
    variant: {
      success: "bg-success text-success-foreground hover:bg-success",
      warning: "bg-warning text-warning-foreground hover:bg-warning",
      error: "bg-error text-error-foreground hover:bg-error",
      display: "bg-gray-500 hover:bg-gray-500"
    }
  }
});

type BadgeType = "success" | "warning" | "error" | "display" | null | undefined;
type BadgeLabelProps = Omit<BadgeProps, "variant"> & {
  type: BadgeType ;
};

const BadgeLabel: Component<BadgeLabelProps> = (props) => {
  const [local, others] = splitProps(props, ["class", "children", "type"]);

  return (
    <Badge
      class={cn(badgeDeltaVariants({ variant: local.type }), local.class)}
      {...others}
    >
      <span class="flex gap-1">
        {local.children}
      </span>
    </Badge>
  );
};

export { BadgeLabel };
export type { BadgeType };
