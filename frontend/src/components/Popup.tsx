import { Component, JSX } from 'solid-js';
import { AlertDialog, AlertDialogContent, AlertDialogDescription, AlertDialogTitle, AlertDialogTrigger } from '~/components/ui/alert-dialog';

interface MxPopupProps {
	title: string;
	open: boolean;
	onOpenChange?: (o: boolean) => void;
	children?: JSX.Element;
};

export const MxPopup: Component<MxPopupProps> = (props) => {
	return (
		<AlertDialog open={props.open} onOpenChange={props.onOpenChange}>
			<AlertDialogTrigger></AlertDialogTrigger>
			<AlertDialogContent class="w-3/4 max-w-svh">
				<AlertDialogTitle>{props.title}</AlertDialogTitle>
				<AlertDialogDescription>{props.children}</AlertDialogDescription>
			</AlertDialogContent>
		</AlertDialog>
	);
};
