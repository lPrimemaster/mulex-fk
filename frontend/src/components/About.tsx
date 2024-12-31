import { Component, JSX } from 'solid-js';
import { AlertDialog, AlertDialogContent, AlertDialogDescription, AlertDialogTitle, AlertDialogTrigger } from '~/components/ui/alert-dialog';

interface SetShowF {
	(set: boolean): void;
};

const About: Component<{show: boolean, setShow: SetShowF, children? : JSX.Element}> = (props) => {
	return (
		<AlertDialog open={props.show} onOpenChange={props.setShow}>
			<AlertDialogTrigger></AlertDialogTrigger>
			<AlertDialogContent>
				<AlertDialogTitle>About</AlertDialogTitle>
				<AlertDialogDescription>
					<h2 class="font-bold">mxfk 2024-2025</h2>
					<span class="font-medium">Current Version: {__APP_VERSION__} ({__APP_VNAME__})</span>
					<br/>
					Licensed under the GNU GPL v3.

					<br/><br/>
					<h2 class="font-bold">Dependencies</h2>
					<li>SQLite - [Public Domain]</li>
					<li>RapidJSON - Copyright (c) 2006-2013 Alexander Chemeris [BSD License]</li>
					<li>Libusb - [GNU LGPLv2.1]</li>
					<li>uSockets - [Apache License v2.0]</li>
					<li>uWebSockets - [Apache License v2.0]</li>
					<li>base64 - Copyright (c) Nick Galbreath, Wojciech Muła, Matthieu Darbois & Alfred Klomp [BSD License]</li>
				</AlertDialogDescription>
			</AlertDialogContent>
		</AlertDialog>
	);
};

export default About;
